#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "stickyaim/core/config_io.hpp"
#include "license_ui.hpp"
#include "stickyaim/platform/license.hpp"
#include "stickyaim/platform/system_probe.hpp"
#include "stickyaim/platform/vigem_sink.hpp"
#include "widgets/grouped_list.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

// English key; shown through T() at the point of use.
const char* kRiskText =
    "Virtual controller output creates a second (virtual) Xbox 360 controller and sends the PROCESSED stick values to it.\n\n"
    "Before enabling it, understand that:\n"
    "• Many online games, including Call of Duty, prohibit unauthorized software or devices that modify controller input. "
    "Activision's RICOCHET anti-cheat targets input-modifying devices (e.g. Cronus Zen, XIM) and in 2026 announced "
    "detections based on input timing and patterns. Using this output in online play can put your account at risk of a ban. Upgrade Aim does not and will "
    "not attempt to hide itself from anti-cheat software.\n"
    "• It requires the ViGEmBus driver, which you install yourself. Its project was retired in 2023 and receives no updates.\n"
    "• Without hiding the physical controller (e.g. HidHide), the game may receive BOTH controllers (double input).\n"
    "• Upgrade Aim never changes the game's own aim assist. Any improvement must be measured, not assumed.\n\n"
    "Safer alternatives: use the Stick Lab and Testing pages to find values, then enter them in the game's own controller "
    "settings or in the controller manufacturer's official software.\n\n"
    "Enable virtual controller output?";

}  // namespace

SettingsPage::SettingsPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    QVBoxLayout* col = ui::centeredColumn(this);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::heading(T("Settings")));
    titles->addWidget(ui::subheading(T("Where the processed input goes, safety controls and app data.")));
    col->addLayout(titles);

    // ---- output
    auto* out = new ui::Group(T("Output"));
    allowVirtual_ = new QCheckBox;
    out->addRow(T("Allow virtual controller"), allowVirtual_, T("Off by default. Creates a virtual Xbox 360 controller (ViGEm)."));
    output_ = new ui::Segmented({T("Preview"), T("Virtual controller")});
    out->addRow(T("Send input to"), output_);
    forwardRumble_ = new QCheckBox;
    forwardRumble_->setChecked(true);
    forwardRumble_->setEnabled(false);
    out->addRow(T("Forward rumble to the controller"), forwardRumble_, T("Always on in this version."));
    auto* hideBox = new QWidget;
    hideBox->setObjectName("groupRow");
    auto* hideL = new QHBoxLayout(hideBox);
    hideL->setContentsMargins(0, 0, 0, 0);
    hideL->setSpacing(8);
    auto* hideBtn = new QPushButton(T("Set up"));
    hideBtn->setProperty("primary", true);
    auto* unhideBtn = new QPushButton(T("Undo"));
    hideL->addWidget(hideBtn);
    hideL->addWidget(unhideBtn);
    out->addRow(T("Hide the controller from games"), hideBox,
                T("Uses HidHide so games only see Upgrade Aim's virtual controller. Windows asks for administrator permission."));
    outputStatus_ = out->footerLabel();
    col->addWidget(out);
    auto runHide = [this](bool undo) {
        bool ok = false;
        const QString msg = app_->setupHidHide(undo, &ok);
        if (ok) QMessageBox::information(this, T("Hide the controller from games"), msg);
        else QMessageBox::warning(this, T("Hide the controller from games"), msg);
        refresh();
    };
    connect(hideBtn, &QPushButton::clicked, this, [runHide] { runHide(false); });
    connect(unhideBtn, &QPushButton::clicked, this, [runHide] { runHide(true); });

    // ---- safety
    auto* safety = new ui::Group(T("Safety"));
    panicEnabled_ = new QCheckBox;
    safety->addRow(T("Panic combo"), panicEnabled_, T("Hold Back + Start on the controller to switch Bypass on or off."));
    panicHold_ = new QSpinBox;
    panicHold_->setRange(300, 5000);
    panicHold_->setSingleStep(100);
    panicHold_->setSuffix(" ms");
    panicHold_->setKeyboardTracking(false);
    safety->addRow(T("Hold time"), panicHold_);
    safety->setFooter(T("Ctrl+Alt+P switches Bypass on or off. Ctrl+Alt+X removes the virtual controller at once and returns to preview."));
    col->addWidget(safety);

    // ---- data
    auto* dataGroup = new ui::Group(T("Data"));
    auto* exportBtn = new QPushButton(T("Export…"));
    dataGroup->addRow(T("Export profile and calibration"), exportBtn);
    auto* importBtn = new QPushButton(T("Import…"));
    dataGroup->addRow(T("Import configuration or profile"), importBtn);
    auto* openBtn = new QPushButton(T("Open"));
    dataGroup->addRow(T("Data folder"), openBtn, app_->userDataDir());
    auto* resetBtn = new QPushButton(T("Reset…"));
    resetBtn->setObjectName("danger");
    dataGroup->addRow(T("Reset to defaults"), resetBtn, T("Settings, calibration and the active profile. Saved profile files are kept."));
    col->addWidget(dataGroup);

    // ---- about
    auto* about = new ui::Group(T("About"));
    auto* version = new QLabel(QApplication::applicationVersion());
    version->setObjectName("rowSubtitle");
    about->addRow("Upgrade Aim", version, T("Controller precision lab · GameSir Tarantula 8K"));
    auto* lang = new QLabel(i18n::language() == i18n::Lang::En ? "English" : "Português (Brasil)");
    lang->setObjectName("rowSubtitle");
    about->addRow(T("Language"), lang, T("Change it on the start screen."));
    const auto lic = sa::win::license::installedLicense();
    auto* licLabel = new QLabel(!lic.valid ? T("Not activated yet.")
                                : lic.timed() ? QString(T("Test license until %1 (%2 left)")).arg(license_ui::expiryText(lic), license_ui::remainingText(lic))
                                                : T("Active, no expiry"));
    licLabel->setObjectName("rowSubtitle");
    about->addRow(T("License"), licLabel, QString(T("This machine ID: %1")).arg(QString::fromStdString(sa::win::license::machineId())));
    col->addWidget(about);
    col->addStretch();

    connect(allowVirtual_, &QCheckBox::toggled, this, [this](bool on) {
        if (on && !app_->allowVirtualOutput()) {
            const auto answer = QMessageBox::warning(this, T("Virtual controller output"), T(kRiskText), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                allowVirtual_->blockSignals(true);
                allowVirtual_->setChecked(false);
                allowVirtual_->blockSignals(false);
                return;
            }
        }
        app_->settings()["allow_virtual_output"] = on;
        app_->saveSettings();
        if (!on && app_->output() == AppController::OutputKind::ViGEm) app_->setOutput(AppController::OutputKind::Preview, nullptr);
        refresh();
    });
    connect(output_, &ui::Segmented::currentChanged, this, [this](int idx) {
        QString err;
        if (!app_->setOutput(idx == 1 ? AppController::OutputKind::ViGEm : AppController::OutputKind::Preview, &err))
            QMessageBox::warning(this, T("Output"), err);
        refresh();
    });
    // Changes apply immediately, without an Apply button.
    auto applyPanic = [this] {
        auto c = app_->engineConfig();
        if (c.panic.enabled == panicEnabled_->isChecked() && c.panic.holdMs == panicHold_->value()) return;
        c.panic.enabled = panicEnabled_->isChecked();
        c.panic.holdMs = panicHold_->value();
        app_->setEngineConfig(c);
    };
    connect(panicEnabled_, &QCheckBox::toggled, this, applyPanic);
    connect(panicHold_, &QSpinBox::valueChanged, this, applyPanic);
    connect(exportBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, T("Export configuration"), app_->userDataDir() + "/upgradeaim_config.json", T("JSON (*.json)"));
        if (path.isEmpty()) return;
        nlohmann::json j = {{"format", "stickyaim-config"},
                            {"version", 1},
                            {"profile", sa::toJson(app_->profile())},
                            {"calibration", sa::toJson(app_->calibration())}};
        std::string err;
        if (!sa::writeTextFile(path.toStdString(), j.dump(2) + "\n", &err)) QMessageBox::warning(this, T("Export"), QString::fromStdString(err));
    });
    connect(importBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, T("Import configuration or profile"), app_->userDataDir(), T("JSON (*.json)"));
        if (path.isEmpty()) return;
        std::string text, err;
        if (!sa::readTextFile(path.toStdString(), text, &err)) {
            QMessageBox::warning(this, T("Import"), QString::fromStdString(err));
            return;
        }
        const auto j = nlohmann::json::parse(text, nullptr, false);
        if (!j.is_object()) {
            QMessageBox::warning(this, T("Import"), T("Not a JSON object."));
            return;
        }
        try {
            const bool bundle = j.value("format", "") == "stickyaim-config";
            const sa::Profile p = sa::profileFromJson(bundle ? j["profile"] : j);
            const auto errs = sa::validate(p);
            if (!errs.empty()) {
                QMessageBox::warning(this, T("Import"), T("Profile does not validate: ") + QString::fromStdString(errs.front()));
                return;
            }
            app_->setProfile(p);
            if (bundle && j.contains("calibration")) app_->setCalibration(sa::calibrationFromJson(j["calibration"]));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, T("Import"), e.what());
        }
    });
    connect(resetBtn, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, T("Reset"), T("Reset settings, calibration and the active profile to defaults? Saved profile files are kept.")) !=
            QMessageBox::Yes)
            return;
        app_->resetAllSettings();
        app_->setCalibration({});
        app_->setProfile(sa::Profile{});
        refresh();
    });
    connect(openBtn, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(app_->userDataDir())); });
    connect(app_, &AppController::engineStateChanged, this, &SettingsPage::refresh);
    refresh();
}

void SettingsPage::refresh() {
    allowVirtual_->blockSignals(true);
    allowVirtual_->setChecked(app_->allowVirtualOutput());
    allowVirtual_->blockSignals(false);
    output_->setEnabled(app_->allowVirtualOutput());
    output_->setCurrent(app_->output() == AppController::OutputKind::ViGEm ? 1 : 0);
    const auto c = app_->engineConfig();
    panicEnabled_->blockSignals(true);
    panicHold_->blockSignals(true);
    panicEnabled_->setChecked(c.panic.enabled);
    panicHold_->setValue(c.panic.holdMs);
    panicEnabled_->blockSignals(false);
    panicHold_->blockSignals(false);
    const auto drv = sa::win::probeDrivers();
    QString status = QString(T("ViGEmBus: %1 · HidHide: %2 · ViGEm client in this build: %3"))
                         .arg(T((drv.vigemBus.state).c_str()))
                         .arg(T((drv.hidHide.state).c_str()))
                         .arg(sa::win::ViGEmX360Sink::isCompiledIn() ? T("yes") : T("no"));
    if (app_->output() == AppController::OutputKind::ViGEm && app_->engineRunning())
        status += QString(T("<br><b style='color:#FF9F0A'>Virtual controller active.</b>"));
    status += "<br>" + T("If the game starts from Steam, turn Steam Input off for it: Library → game → Properties → Controller.");
    outputStatus_->setText(status);
    outputStatus_->show();
}