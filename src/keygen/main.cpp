// Upgrade Aim code generator — for the app's owner only. Never ship it.
//
// Holds the private signing key in %APPDATA%\UpgradeAim Keygen\private.key
// and issues activation codes. GUI by default; command line:
//   upgradeaim-keygen --init                 create the key pair and write the
//                                            public key into the app sources
//   upgradeaim-keygen --issue [--machine ID] [--days N] [--name TEXT]
//   upgradeaim-keygen --machine-id           print this PC's machine ID
#include <windows.h>
#include <shellapi.h>

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <fstream>
#include <random>

#include "stickyaim/platform/license.hpp"

namespace lic = sa::win::license;

namespace {

QString keyDir() {
    const QString dir = QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA")) + "/UpgradeAim Keygen";
    QDir().mkpath(dir);
    return dir;
}

bool loadPrivateKey(std::vector<std::uint8_t>& key) {
    QFile f(keyDir() + "/private.key");
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray b = f.readAll();
    key.assign(b.begin(), b.end());
    return !key.empty();
}

QString writePublicKeyInclude(const std::vector<std::uint8_t>& pub) {
    QString text = "// Written by `upgradeaim-keygen --init`. Public half of the signing key; safe to commit.\n"
                   "static const unsigned char kLicensePublicKey[] = {";
    for (size_t i = 0; i < pub.size(); ++i) text += (i % 12 == 0 ? "\n    " : " ") + QString("0x%1,").arg(pub[i], 2, 16, QChar('0'));
    text += QString("\n};\nstatic const std::size_t kLicensePublicKeySize = %1;\n").arg(pub.size());
    const QString path = QStringLiteral(STICKYAIM_SOURCE_DIR) + "/src/platform/license_public_key.inc";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
    f.write(text.toUtf8());
    return path;
}

// Creates the key pair once. Returns an empty string on success.
QString init(bool* created) {
    *created = false;
    std::vector<std::uint8_t> priv;
    if (loadPrivateKey(priv)) return {};
    std::vector<std::uint8_t> pub;
    std::string err;
    if (!lic::generateKeyPair(priv, pub, &err)) return QString::fromStdString(err);
    QFile f(keyDir() + "/private.key");
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return "could not create " + f.fileName();
    f.write(reinterpret_cast<const char*>(priv.data()), static_cast<qint64>(priv.size()));
    f.close();
    if (writePublicKeyInclude(pub).isEmpty()) return "could not write the public key into the app sources";
    *created = true;
    return {};
}

std::uint32_t nextSerial() {
    QFile f(keyDir() + "/issued.csv");
    std::uint32_t n = 1;
    if (f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            const auto line = f.readLine();
            if (!line.startsWith("serial")) ++n;
        }
    }
    return n;
}

QString issueAndLog(const QString& name, const QString& machineText, int days, QString* error) {
    std::vector<std::uint8_t> priv;
    if (!loadPrivateKey(priv)) {
        *error = "No private key yet. Run --init first.";
        return {};
    }
    std::optional<lic::MachineHash> machine;
    if (!machineText.trimmed().isEmpty()) {
        machine = lic::parseMachineId(machineText.toStdString());
        if (!machine) {
            *error = "The machine ID must have 16 hexadecimal characters (XXXX-XXXX-XXXX-XXXX).";
            return {};
        }
    }
    const int expiry = days > 0 ? lic::today() + days : 0;
    const std::uint32_t serial = nextSerial();
    std::string err;
    const QString code = QString::fromStdString(lic::issue(priv, serial, machine, expiry, &err));
    if (code.isEmpty()) {
        *error = QString::fromStdString(err);
        return {};
    }
    QFile log(keyDir() + "/issued.csv");
    const bool fresh = !log.exists();
    if (log.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&log);
        if (fresh) out << "serial;date;name;machine;expires;code\n";
        out << serial << ";" << QDate::currentDate().toString(Qt::ISODate) << ";" << QString(name).replace(';', ',') << ";"
            << (machine ? machineText.trimmed().toUpper() : QString("any")) << ";"
            << (expiry ? QString::fromStdString(lic::dayToString(expiry)) : QString("never")) << ";" << code << "\n";
    }
    return code;
}

int runCli(const QStringList& args) {
    if (args.contains("--machine-id")) {
        std::printf("%s\n", lic::machineId().c_str());
        return 0;
    }
    if (args.contains("--init")) {
        bool created = false;
        const QString err = init(&created);
        if (!err.isEmpty()) {
            std::fprintf(stderr, "init failed: %s\n", qPrintable(err));
            return 1;
        }
        std::printf(created ? "Key pair created in %s. Rebuild the app.\n" : "Key pair already exists in %s.\n", qPrintable(keyDir()));
        return 0;
    }
    auto value = [&](const QString& flag) {
        const auto i = args.indexOf(flag);
        return i >= 0 && i + 1 < args.size() ? args[i + 1] : QString();
    };
    QString error;
    const QString code = issueAndLog(value("--name"), value("--machine"), value("--days").toInt(), &error);
    if (code.isEmpty()) {
        std::fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    std::printf("%s\n", qPrintable(code));
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QStringList args = QApplication::arguments();
    if (args.contains("--init") || args.contains("--issue") || args.contains("--machine-id")) {
        // Keep redirected output (pipe/file); otherwise print to the console
        // this tool was started from.
        const DWORD type = GetFileType(GetStdHandle(STD_OUTPUT_HANDLE));
        if (type != FILE_TYPE_PIPE && type != FILE_TYPE_DISK && AttachConsole(ATTACH_PARENT_PROCESS)) {
            std::freopen("CONOUT$", "w", stdout);
            std::freopen("CONOUT$", "w", stderr);
        }
        return runCli(args);
    }

    bool created = false;
    if (const QString err = init(&created); !err.isEmpty()) {
        QMessageBox::critical(nullptr, "Upgrade Aim — gerador", err);
        return 1;
    }
    if (created)
        QMessageBox::information(nullptr, "Upgrade Aim — gerador",
                                 "Chave criada em " + keyDir() + ".\n\nFaça backup do arquivo private.key: sem ele não dá para gerar "
                                 "novos códigos. Recompile o app para ele reconhecer esta chave.");

    QWidget w;
    w.setWindowTitle("Upgrade Aim — gerador de códigos");
    w.setStyleSheet("QWidget { background: #1C1C1F; color: #F5F5F7; font-family: 'Segoe UI Variable Text', 'Segoe UI'; font-size: 13px; }"
                    "QLineEdit, QSpinBox, QPlainTextEdit { background: #2A2A2E; border: 1px solid transparent; border-radius: 7px; padding: 6px 9px; }"
                    "QLineEdit:focus, QSpinBox:focus { border: 1px solid #0A84FF; }"
                    "QPushButton { background: #0A84FF; color: white; border: none; border-radius: 8px; padding: 8px 18px; font-weight: 600; }"
                    "QPushButton#secondary { background: #2A2A2E; font-weight: 500; }"
                    "QLabel#muted { color: #8E8E93; font-size: 12px; }");
    auto* root = new QVBoxLayout(&w);
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(14);
    auto* title = new QLabel("Gerar código de ativação");
    title->setStyleSheet("font-size: 20px; font-weight: 700;");
    root->addWidget(title);
    auto* form = new QFormLayout;
    form->setSpacing(10);
    auto* name = new QLineEdit;
    name->setPlaceholderText("Nome ou e-mail do cliente (só para o seu registro)");
    auto* machine = new QLineEdit;
    machine->setPlaceholderText("XXXX-XXXX-XXXX-XXXX (vazio = qualquer PC)");
    auto* days = new QSpinBox;
    days->setRange(0, 3650);
    days->setSuffix(" dias");
    days->setSpecialValueText("Sem validade");
    form->addRow("Cliente", name);
    form->addRow("ID da máquina", machine);
    form->addRow("Validade", days);
    root->addLayout(form);
    auto* hint = new QLabel("O cliente vê o ID da máquina na tela de ativação do app. Com o ID preenchido, o código só funciona "
                            "naquele PC.");
    hint->setObjectName("muted");
    hint->setWordWrap(true);
    root->addWidget(hint);
    auto* out = new QPlainTextEdit;
    out->setReadOnly(true);
    out->setFixedHeight(90);
    root->addWidget(out);
    auto* buttons = new QHBoxLayout;
    auto* gen = new QPushButton("Gerar código");
    auto* copy = new QPushButton("Copiar");
    copy->setObjectName("secondary");
    auto* openDir = new QPushButton("Abrir pasta das chaves");
    openDir->setObjectName("secondary");
    buttons->addWidget(gen);
    buttons->addWidget(copy);
    buttons->addStretch();
    buttons->addWidget(openDir);
    root->addLayout(buttons);
    auto* foot = new QLabel("Todos os códigos ficam registrados em issued.csv, na pasta das chaves. Nunca envie o private.key a ninguém.");
    foot->setObjectName("muted");
    foot->setWordWrap(true);
    root->addWidget(foot);

    QObject::connect(gen, &QPushButton::clicked, &w, [&] {
        QString error;
        const QString code = issueAndLog(name->text(), machine->text(), days->value(), &error);
        if (code.isEmpty()) {
            QMessageBox::warning(&w, "Upgrade Aim — gerador", error);
            return;
        }
        out->setPlainText(code);
    });
    QObject::connect(copy, &QPushButton::clicked, &w, [&] { QGuiApplication::clipboard()->setText(out->toPlainText()); });
    QObject::connect(openDir, &QPushButton::clicked, &w, [] {
        ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(keyDir().utf16()), nullptr, nullptr, SW_SHOWNORMAL);
    });
    w.resize(620, 420);
    w.show();
    return app.exec();
}
