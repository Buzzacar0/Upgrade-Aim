#include "launcher.hpp"

#include <QClipboard>
#include <QFileDialog>
#include <QGuiApplication>
#include <QLineEdit>
#include <QFileInfo>
#include <QFrame>
#include <QIcon>
#include <QRadialGradient>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include "license_ui.hpp"
#include "stickyaim/platform/license.hpp"
#include "stickyaim/platform/win_util.hpp"
#include "widgets/ui_helpers.hpp"

namespace {

QString iniPath() { return QString::fromStdString(sa::win::userDataDir()) + "/launcher.ini"; }

constexpr double kImageOpacity = 0.5;

// Launcher text is shown in the language being chosen, so it is kept here
// rather than in the T() table.
QString L(i18n::Lang lang, const char* pt, const char* en) { return QString::fromUtf8(lang == i18n::Lang::En ? en : pt); }

// Every code is "UA1-" plus 25 groups of 5 characters and one of 3. What gets
// pasted may carry more (the "Expira em" line, a chat message, a second paste)
// or miss the "UA1-" start, so the field looks for that shape in the text.
QStringList codeCandidates(const QString& pasted) {
    QString text = pasted.toUpper();
    text.remove(QRegularExpression("\\s"));
    QStringList out;
    static const QRegularExpression full("UA1(?:-[0-9A-Z]{5}){25}-[0-9A-Z]{3}");
    for (auto it = full.globalMatch(text); it.hasNext();) out << it.next().captured(0);
    static const QRegularExpression bare("(?:[0-9A-Z]{5}-){25}[0-9A-Z]{3}");
    for (auto it = bare.globalMatch(text); it.hasNext();) {
        const QString code = "UA1-" + it.next().captured(0);
        if (!out.contains(code)) out << code;
    }
    return out;
}

}  // namespace

i18n::Lang Launcher::savedLanguage() {
    QSettings ini(iniPath(), QSettings::IniFormat);
    return i18n::fromCode(ini.value("language", "pt-BR").toString());
}

Launcher::Launcher(QWidget* parent) : QDialog(parent), lang_(savedLanguage()) {
    setWindowTitle("Upgrade Aim");
    setFixedSize(960, 600);
    setWindowIcon(QIcon(":/ui/up-icon.png"));
    ui::applyWindowChrome(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(60, 54, 60, 44);
    root->setSpacing(0);

    auto* mark = new QLabel;
    mark->setPixmap(QIcon(":/ui/up-icon.png").pixmap(64, 64));
    root->addWidget(mark);
    root->addSpacing(22);
    auto* title = new QLabel("Upgrade Aim");
    title->setStyleSheet("font-size:46px; font-weight:700; letter-spacing:-0.5px; color:#F5F5F7; background:transparent;");
    subtitle_ = new QLabel;
    subtitle_->setStyleSheet("font-size:17px; color:#A1A1A6; background:transparent;");
    root->addWidget(title);
    root->addSpacing(4);
    root->addWidget(subtitle_);
    root->addStretch(1);

    languageLabel_ = new QLabel;
    languageLabel_->setStyleSheet("font-size:12px; font-weight:600; color:#98989D; background:transparent;");
    root->addWidget(languageLabel_);
    root->addSpacing(8);

    // Segmented control.
    auto* segment = new QFrame;
    segment->setObjectName("segment");
    segment->setStyleSheet("QFrame#segment { background: rgba(255,255,255,0.08); border-radius: 9px; }"
                           "QFrame#segment QPushButton { background: transparent; color: #D1D1D6; border: none; border-radius: 7px;"
                           " padding: 7px 22px; font-size: 13px; font-weight: 500; }"
                           "QFrame#segment QPushButton:hover:!checked { background: rgba(255,255,255,0.05); }"
                           "QFrame#segment QPushButton:checked { background: rgba(255,255,255,0.20); color: #FFFFFF; }");
    auto* segL = new QHBoxLayout(segment);
    segL->setContentsMargins(2, 2, 2, 2);
    segL->setSpacing(2);
    pt_ = new QPushButton("Português (Brasil)");
    en_ = new QPushButton("English");
    for (auto* b : {pt_, en_}) {
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        segL->addWidget(b);
    }
    auto* langRow = new QHBoxLayout;
    langRow->addWidget(segment);
    langRow->addStretch();
    root->addLayout(langRow);
    root->addSpacing(26);

    // Activation: shown until a valid code is stored, in place of Start.
    activation_ = new QWidget;
    activation_->setStyleSheet("background: transparent;");
    auto* act = new QVBoxLayout(activation_);
    act->setContentsMargins(0, 0, 0, 0);
    act->setSpacing(8);
    activationTitle_ = new QLabel;
    activationTitle_->setStyleSheet("font-size:12px; font-weight:600; color:#98989D; background:transparent;");
    act->addWidget(activationTitle_);
    auto* idRow = new QHBoxLayout;
    idRow->setSpacing(12);
    machineLabel_ = new QLabel;
    machineLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    machineLabel_->setStyleSheet("font-size:14px; color:#F5F5F7; background:transparent;");
    copyId_ = new QPushButton;
    copyId_->setCursor(Qt::PointingHandCursor);
    copyId_->setStyleSheet("QPushButton { font-size:13px; padding:4px 0; border:none; background:transparent; color:#B44DFF; }"
                           "QPushButton:hover { color:#C77DFF; }");
    idRow->addWidget(machineLabel_);
    idRow->addWidget(copyId_);
    idRow->addStretch();
    act->addLayout(idRow);
    auto* codeRow = new QHBoxLayout;
    codeRow->setSpacing(10);
    code_ = new QLineEdit;
    code_->setMinimumWidth(520);
    code_->setStyleSheet("QLineEdit { background: rgba(255,255,255,0.08); color:#F5F5F7; border: 1px solid transparent;"
                         " border-radius: 8px; padding: 8px 10px; font-size: 13px; }"
                         "QLineEdit:focus { border: 1px solid #B44DFF; }");
    activate_ = new QPushButton;
    activate_->setCursor(Qt::PointingHandCursor);
    activate_->setStyleSheet("QPushButton { font-size:14px; font-weight:600; padding:8px 26px; border:none; border-radius:8px;"
                             " background:#B44DFF; color:#FFFFFF; } QPushButton:hover { background:#C36BFF; }");
    codeRow->addWidget(code_, 1);
    codeRow->addWidget(activate_);
    act->addLayout(codeRow);
    activationError_ = new QLabel;
    activationError_->setStyleSheet("font-size:12px; color:#FF453A; background:transparent;");
    act->addWidget(activationError_);
    root->addWidget(activation_);
    root->addSpacing(10);

    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(18);
    start_ = new QPushButton;
    start_->setDefault(true);
    start_->setCursor(Qt::PointingHandCursor);
    start_->setStyleSheet(
        "QPushButton { font-size:15px; font-weight:600; padding:11px 38px; border:none; border-radius:10px;"
        " background:#B44DFF; color:#FFFFFF; }"
        "QPushButton:hover { background:#C36BFF; } QPushButton:pressed { background:#9A2FE6; }");
    bottom->addWidget(start_);
    bottom->addStretch();
    root->addLayout(bottom);
    root->addSpacing(18);

    note_ = new QLabel;
    note_->setWordWrap(true);
    note_->setStyleSheet("font-size:12px; color:#6E6E73; background:transparent;");
    root->addWidget(note_);
    connect(pt_, &QPushButton::clicked, this, [this] { setLanguage(i18n::Lang::PtBr); });
    connect(en_, &QPushButton::clicked, this, [this] { setLanguage(i18n::Lang::En); });
    connect(start_, &QPushButton::clicked, this, [this] {
        if (!license_.valid) return;
        QSettings ini(iniPath(), QSettings::IniFormat);
        ini.setValue("language", i18n::code(lang_));
        accept();
    });
    // Copies only the machine ID.
    connect(copyId_, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(QString::fromStdString(sa::win::license::machineId()));
        copyId_->setText(L(lang_, "Copiado!", "Copied!"));
        QTimer::singleShot(3000, this, [this] { copyId_->setText(L(lang_, "Copiar ID", "Copy ID")); });
    });
    connect(activate_, &QPushButton::clicked, this, &Launcher::activate);
    connect(code_, &QLineEdit::returnPressed, this, &Launcher::activate);

    license_ = sa::win::license::installedLicense();
    loadBackground();
    setLanguage(lang_);
}

void Launcher::activate() {
    const QStringList candidates = codeCandidates(code_->text());
    std::string code = code_->text().trimmed().toStdString();
    auto info = sa::win::license::evaluate(code);
    for (const QString& c : candidates) {
        if (info.valid) break;
        const auto tried = sa::win::license::evaluate(c.toStdString());
        if (tried.valid || c == candidates.first()) {
            code = c.toStdString();
            info = tried;
        }
    }
    if (!info.valid) {
        if (candidates.isEmpty())
            activationError_->setText(L(lang_, "O código está incompleto. No gerador, clique em \"Copiar\" e cole aqui de novo.",
                                        "The code is incomplete. In the generator, click \"Copy\" and paste it here again."));
        else
            activationError_->setText(lang_ == i18n::Lang::En ? QString::fromStdString(info.error) : T(info.error.c_str()));
        return;
    }
    code_->setText(QString::fromStdString(code));
    std::string err;
    if (!sa::win::license::saveLicense(code, &err)) {
        activationError_->setText(QString::fromStdString(err));
        return;
    }
    license_ = info;
    retranslate();
}

void Launcher::setLanguage(i18n::Lang lang) {
    lang_ = lang;
    pt_->setChecked(lang == i18n::Lang::PtBr);
    en_->setChecked(lang == i18n::Lang::En);
    retranslate();
}

void Launcher::retranslate() {
    subtitle_->setText(L(lang_, "Laboratório de precisão para controle · GameSir Tarantula 8K",
                         "Controller precision lab · GameSir Tarantula 8K"));
    languageLabel_->setText(L(lang_, "Idioma", "Language"));
    start_->setText(L(lang_, "Iniciar", "Start"));
    const bool active = license_.valid;
    activation_->setVisible(!active);
    start_->setVisible(active);
    activationTitle_->setText(L(lang_, "ATIVAÇÃO", "ACTIVATION"));
    machineLabel_->setText(L(lang_, "ID desta máquina:  ", "This machine ID:  ") + "<b>" +
                           QString::fromStdString(sa::win::license::machineId()) + "</b>");
    copyId_->setText(L(lang_, "Copiar ID", "Copy ID"));
    code_->setPlaceholderText(L(lang_, "Cole aqui o seu código de ativação (UA1-…)", "Paste your activation code here (UA1-…)"));
    activate_->setText(L(lang_, "Ativar", "Activate"));
    // Explain why the app is locked (expired test code, clock set back, ...).
    if (!active)
        activationError_->setText(license_.error.empty() || license_.error == "Not activated yet."
                                      ? QString()
                                      : (lang_ == i18n::Lang::En ? QString::fromStdString(license_.error) : T(license_.error.c_str())));
    QString licenseText;
    if (active)
        licenseText = license_.timed() ? L(lang_, "Licença de teste até ", "Test license until ") + license_ui::expiryText(license_) +
                                             L(lang_, " (faltam ", " (") + license_ui::remainingText(license_) +
                                             L(lang_, ").", " left).")
                                       : L(lang_, "Licença ativa.", "License active.");
    else
        licenseText = L(lang_, "Clique em \"Copiar ID\", envie o ID e cole aqui o código que você receber.",
                        "Click \"Copy ID\", send the ID and paste the code you receive here.");
    note_->setText(licenseText + "  " +
                   L(lang_, "Começa em pré-visualização segura: nada é enviado ao jogo até você ativar a saída virtual em Configurações.",
                     "Starts in safe preview: nothing is sent to the game until you enable virtual output in Settings."));
}

// One of the two bundled operator pictures, picked at random on each start.
void Launcher::loadBackground() {
    const QString pictures[] = {":/ui/operators-1.jpg", ":/ui/operators-2.jpg"};
    background_.load(pictures[QRandomGenerator::global()->bounded(2)]);
    update();
}

void Launcher::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(0x10, 0x10, 0x12));

    if (!background_.isNull()) {
        // Cover the window, keep the aspect ratio, crop the excess.
        const QPixmap scaled = background_.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QPoint offset((scaled.width() - width()) / 2, (scaled.height() - height()) / 2);
        p.setOpacity(kImageOpacity);
        p.drawPixmap(0, 0, scaled, offset.x(), offset.y(), width(), height());
        p.setOpacity(1.0);
    } else {
        // Built-in backdrop: soft blue and orange glows with faint concentric rings.
        auto glow = [&](QPointF c, double radius, QColor col) {
            QRadialGradient g(c, radius);
            g.setColorAt(0.0, col);
            col.setAlpha(0);
            g.setColorAt(1.0, col);
            p.fillRect(rect(), g);
        };
        glow(QPointF(width() * 0.78, height() * 0.30), width() * 0.55, QColor(180, 77, 255, 70));
        glow(QPointF(width() * 0.95, height() * 0.95), width() * 0.40, QColor(255, 159, 10, 34));
        const QPointF c(width() * 0.78, height() * 0.46);
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 6; ++i) {
            p.setPen(QPen(QColor(255, 255, 255, 22 - i * 3), 1));
            p.drawEllipse(c, 60.0 + i * 46.0, 60.0 + i * 46.0);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 200));
        p.drawEllipse(c, 3, 3);
    }

    // Darken the left side so the text stays readable over any picture.
    QLinearGradient shade(0, 0, width(), 0);
    shade.setColorAt(0.0, QColor(16, 16, 18, 235));
    shade.setColorAt(0.5, QColor(16, 16, 18, 150));
    shade.setColorAt(1.0, QColor(16, 16, 18, 30));
    p.fillRect(rect(), shade);
}