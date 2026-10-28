// Upgrade Aim code generator — for the app's owner only. Never ship it.
//
// Holds the private signing key in %APPDATA%\UpgradeAim Keygen\private.key
// and issues activation codes. GUI by default; command line:
//   upgradeaim-keygen --init                 create the key pair and write the
//                                            public key into the app sources
//   upgradeaim-keygen --issue --machine ID [--minutes N] [--hours N] [--days N] [--name TEXT]
//                                            (no duration = permanent)
//   upgradeaim-keygen --machine-id           print this PC's machine ID
#include <windows.h>
#include <shellapi.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QTimer>
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
#include <QRegularExpression>
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

std::uint16_t nextSerial() {
    QFile f(keyDir() + "/issued.csv");
    std::uint32_t n = 1;
    if (f.open(QIODevice::ReadOnly)) {
        while (!f.atEnd()) {
            const auto line = f.readLine();
            if (!line.startsWith("serial")) ++n;
        }
    }
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(n, 0xFFFF));
}

// Finds XXXX-XXXX-XXXX-XXXX anywhere in a pasted message ("Olá! ... ID é 825C-A5A8-45FB-86BF.").
QString extractMachineId(const QString& text) {
    static const QRegularExpression re("([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})");
    const auto m = re.match(text);
    return m.hasMatch() ? m.captured(0).toUpper() : text.trimmed();
}

QString localTime(std::int64_t minute) {
    return QDateTime::fromSecsSinceEpoch(lic::toUnixSeconds(minute)).toLocalTime().toString("dd/MM/yyyy HH:mm");
}

// durationMinutes 0 = permanent. Returns the code; *expiresMinute is set.
QString issueAndLog(const QString& name, const QString& machineText, std::int64_t durationMinutes, QString* error,
                    std::int64_t* expiresMinute = nullptr) {
    std::vector<std::uint8_t> priv;
    if (!loadPrivateKey(priv)) {
        *error = "Ainda não existe chave privada. Rode --init primeiro.";
        return {};
    }
    const QString machineId = extractMachineId(machineText);
    const auto machine = lic::parseMachineId(machineId.toStdString());
    if (!machine) {
        *error = "O ID da máquina é obrigatório e tem 16 caracteres (XXXX-XXXX-XXXX-XXXX). O cliente vê esse ID na tela de ativação.";
        return {};
    }
    const std::int64_t expires = durationMinutes > 0 ? lic::nowMinute() + durationMinutes : 0;
    if (expiresMinute) *expiresMinute = expires;
    const std::uint16_t serial = nextSerial();
    std::string err;
    const QString code = QString::fromStdString(lic::issue(priv, serial, *machine, expires, &err));
    if (code.isEmpty()) {
        *error = QString::fromStdString(err);
        return {};
    }
    QFile log(keyDir() + "/issued.csv");
    const bool fresh = !log.exists();
    if (log.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&log);
        if (fresh) out << "serial;date;name;machine;expires;code\n";
        out << serial << ";" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") << ";" << QString(name).replace(';', ',')
            << ";" << machineId << ";" << (expires ? localTime(expires) : QString("never")) << ";" << code << "\n";
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
    const std::int64_t minutes = value("--minutes").toLongLong() + value("--hours").toLongLong() * 60 + value("--days").toLongLong() * 1440;
    std::int64_t expires = 0;
    const QString code = issueAndLog(value("--name"), value("--machine"), minutes, &error, &expires);
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
                    "QLineEdit, QSpinBox, QComboBox, QPlainTextEdit { background: #2A2A2E; border: 1px solid transparent; border-radius: 7px; padding: 6px 9px; }"
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
    machine->setPlaceholderText("Cole aqui a mensagem do cliente ou o ID (XXXX-XXXX-XXXX-XXXX)");
    QObject::connect(machine, &QLineEdit::textChanged, &w, [machine](const QString& t) {
        const QString id = extractMachineId(t);
        if (id != t && id.size() == 19) machine->setText(id);  // keep only the ID from a pasted message
    });
    auto* amount = new QSpinBox;
    amount->setRange(1, 100000);
    amount->setValue(1);
    auto* unit = new QComboBox;
    unit->addItems({"minutos", "horas", "dias", "permanente (sem validade)"});
    unit->setCurrentIndex(1);
    auto* timeRow = new QHBoxLayout;
    timeRow->addWidget(amount);
    timeRow->addWidget(unit, 1);
    auto* expiresLabel = new QLabel;
    expiresLabel->setObjectName("muted");
    form->addRow("Cliente", name);
    form->addRow("ID da máquina", machine);
    form->addRow("Tempo de uso", timeRow);
    form->addRow("", expiresLabel);
    root->addLayout(form);
    const std::int64_t unitMinutes[] = {1, 60, 1440, 0};
    auto duration = [=] { return unitMinutes[unit->currentIndex()] * amount->value(); };
    auto updateExpiry = [=] {
        amount->setEnabled(unit->currentIndex() != 3);
        const std::int64_t d = duration();
        expiresLabel->setText(d ? "Se gerar agora, expira em " + localTime(lic::nowMinute() + d) +
                                      ". Quando acabar, o app trava naquele PC e o código não volta a funcionar lá."
                                : "Código permanente: não expira.");
    };
    QObject::connect(amount, &QSpinBox::valueChanged, &w, updateExpiry);
    QObject::connect(unit, &QComboBox::currentIndexChanged, &w, updateExpiry);
    auto* tick = new QTimer(&w);
    QObject::connect(tick, &QTimer::timeout, &w, updateExpiry);
    tick->start(15000);
    updateExpiry();
    auto* hint = new QLabel("No app, o cliente clica em \"Copiar pedido de ativação\" e te manda a mensagem: cole ela inteira no campo "
                            "do ID. O código só funciona naquele PC. "
                            "O tempo conta a partir de agora, não da ativação.");
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
        std::int64_t expires = 0;
        const QString code = issueAndLog(name->text(), machine->text(), duration(), &error, &expires);
        if (code.isEmpty()) {
            QMessageBox::warning(&w, "Upgrade Aim — gerador", error);
            return;
        }
        out->setPlainText(code + "\n\n" + (expires ? "Expira em " + localTime(expires) : QString("Permanente")));
    });
    QObject::connect(copy, &QPushButton::clicked, &w, [&] { QGuiApplication::clipboard()->setText(out->toPlainText().section('\n', 0, 0)); });
    QObject::connect(openDir, &QPushButton::clicked, &w, [] {
        ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(keyDir().utf16()), nullptr, nullptr, SW_SHOWNORMAL);
    });
    w.resize(620, 420);
    w.show();
    return app.exec();
}
