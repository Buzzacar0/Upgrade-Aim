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
#include "stickyaim/platform/system_probe.hpp"
#include "stickyaim/platform/vigem_sink.hpp"
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
    auto* root = new QVBoxLayout(this);
    root->addWidget(ui::heading(T("Settings")));

    auto* outBox = new QGroupBox(T("Input delivery"));
    auto* outL = new QVBoxLayout(outBox);
    allowVirtual_ = new QCheckBox(T("Allow virtual controller output (ViGEm) — off by default"));
    output_ = new QComboBox;
    output_->addItems({T("Safe preview (no injection)"), T("Virtual Xbox 360 controller (ViGEm)")});
    forwardRumble_ = new QCheckBox(T("Forward game rumble from the virtual pad to the physical controller"));
    forwardRumble_->setChecked(true);
    forwardRumble_->setEnabled(false);
    forwardRumble_->setToolTip(T("Always on in this version."));
    outputStatus_ = new QLabel;
    outputStatus_->setWordWrap(true);
    outL->addWidget(allowVirtual_);
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(T("Output")));
    row->addWidget(output_, 1);
    outL->addLayout(row);
    outL->addWidget(forwardRumble_);
    outL->addWidget(outputStatus_);
    outL->addWidget(ui::note(T("Emergency controls: Ctrl+Alt+P toggles pass-through (raw input), Ctrl+Alt+X immediately removes the "
                             "virtual controller and returns to safe preview. Holding the panic combo on the controller toggles "
                             "pass-through.")));
    root->addWidget(outBox);

    auto* panicBox = new QGroupBox(T("Controller panic combo"));
    auto* panicL = new QFormLayout(panicBox);
    panicEnabled_ = new QCheckBox(T("Hold Back + Start to toggle pass-through"));
    panicHold_ = new QSpinBox;
    panicHold_->setRange(300, 5000);
    panicHold_->setSuffix(" ms");
    panicL->addRow(panicEnabled_);
    panicL->addRow(T("Hold time"), panicHold_);
    auto* applyPanic = new QPushButton(T("Apply"));
    panicL->addRow(applyPanic);
    root->addWidget(panicBox);

    auto* dataBox = new QGroupBox(T("Configuration"));
    auto* dataL = new QHBoxLayout(dataBox);
    auto* exportBtn = new QPushButton(T("Export profile + calibration…"));
    auto* importBtn = new QPushButton(T("Import…"));
    auto* resetBtn = new QPushButton(T("Reset everything to defaults"));
    resetBtn->setObjectName("danger");
    auto* openBtn = new QPushButton(T("Open data folder (logs, profiles)"));
    dataL->addWidget(exportBtn);
    dataL->addWidget(importBtn);
    dataL->addWidget(openBtn);
    dataL->addStretch();
    dataL->addWidget(resetBtn);
    root->addWidget(dataBox);
    root->addWidget(ui::note(T("Data folder: ") + app_->userDataDir()));
    root->addStretch();

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
    connect(output_, &QComboBox::activated, this, [this](int idx) {
        QString err;
        if (!app_->setOutput(idx == 1 ? AppController::OutputKind::ViGEm : AppController::OutputKind::Preview, &err))
            QMessageBox::warning(this, T("Output"), err);
        refresh();
    });
    connect(applyPanic, &QPushButton::clicked, this, [this] {
        auto c = app_->engineConfig();
        c.panic.enabled = panicEnabled_->isChecked();
        c.panic.holdMs = panicHold_->value();
        app_->setEngineConfig(c);
    });
    connect(exportBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, T("Export configuration"), app_->userDataDir() + "/stickyaim_config.json", T("JSON (*.json)"));
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
    output_->setCurrentIndex(app_->output() == AppController::OutputKind::ViGEm ? 1 : 0);
    const auto c = app_->engineConfig();
    panicEnabled_->setChecked(c.panic.enabled);
    panicHold_->setValue(c.panic.holdMs);
    const auto drv = sa::win::probeDrivers();
    QString status = QString(T("ViGEm client compiled in: %1 · ViGEmBus driver: %2 · HidHide: %3"))
                         .arg(sa::win::ViGEmX360Sink::isCompiledIn() ? T("yes") : T("no"))
                         .arg(T((drv.vigemBus.state).c_str()))
                         .arg(T((drv.hidHide.state).c_str()));
    if (app_->output() == AppController::OutputKind::ViGEm && app_->engineRunning()) {
        status += QString(T("<br><b style='color:#FF9F0A'>Virtual controller active.</b>"));
    }
    outputStatus_->setText(status);
}
