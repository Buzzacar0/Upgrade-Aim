#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "stickyaim/core/config_io.hpp"
#include "stickyaim/platform/rawinput.hpp"
#include "stickyaim/platform/system_probe.hpp"
#include "stickyaim/platform/vigem_sink.hpp"
#include "stickyaim/platform/xinput_source.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

struct CheckItem {
    const char* launcher;
    const char* test;
};

// Manual checks the user performs and records. Nothing here is pre-filled.
const CheckItem kChecks[] = {
    {"steam", "Game reacts to the physical controller (Steam Input OFF for the game)"},
    {"steam", "Game reacts to the physical controller (Steam Input ON for the game)"},
    {"steam", "Game reacts to the StickyAim virtual pad"},
    {"steam", "No double input while the virtual pad is active"},
    {"steam", "Rumble reaches the physical controller"},
    {"battlenet", "Game reacts to the physical controller"},
    {"battlenet", "Game reacts to the StickyAim virtual pad"},
    {"battlenet", "No double input while the virtual pad is active"},
    {"battlenet", "Rumble reaches the physical controller"},
    {"xbox", "Game reacts to the physical controller"},
    {"xbox", "Game reacts to the StickyAim virtual pad"},
    {"xbox", "No double input while the virtual pad is active"},
    {"xbox", "Rumble reaches the physical controller"},
};

// Stored in settings in English; shown through T().
const char* const kResults[] = {"Untested", "Works", "Problem", "Not applicable"};

QTableWidget* makeTable(const QStringList& headers) {
    auto* t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(110);
    return t;
}

void setRow(QTableWidget* t, int row, const QStringList& cells) {
    for (int c = 0; c < cells.size(); ++c) t->setItem(row, c, new QTableWidgetItem(cells[c]));
}

QString launcherName(const QString& id) {
    if (id == "steam") return T("Steam");
    if (id == "battlenet") return T("Battle.net");
    return T("Xbox app");
}

}  // namespace

CompatPage::CompatPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* outer = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    top->addWidget(ui::heading(T("Device Compatibility")));
    top->addStretch();
    auto* rescanBtn = new QPushButton(T("Re-scan"));
    auto* exportBtn = new QPushButton(T("Export report…"));
    top->addWidget(rescanBtn);
    top->addWidget(exportBtn);
    outer->addLayout(top);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* content = new QWidget;
    auto* root = new QVBoxLayout(content);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    summary_ = new QLabel;
    summary_->setWordWrap(true);
    summary_->setTextFormat(Qt::RichText);
    root->addWidget(summary_);

    auto box = [&](const QString& title, QWidget* w, const QString& noteText = {}) {
        auto* g = new QGroupBox(title);
        auto* l = new QVBoxLayout(g);
        l->addWidget(w);
        if (!noteText.isEmpty()) l->addWidget(ui::note(noteText));
        root->addWidget(g);
    };
    xinput_ = makeTable({T("Slot"), T("Connected"), T("Subtype"), T("Wireless"), T("Note")});
    box(T("XInput slots"), xinput_,
        T("XInput does not expose which physical device occupies a slot. When the virtual pad is active it takes its own slot."));
    hid_ = makeTable({T("VID:PID"), T("Interface"), T("Usage"), T("Product"), T("Known device / mode"), T("Verification")});
    box(T("HID interfaces of connected controllers"), hid_);
    drivers_ = makeTable({T("Component"), T("Status"), T("Needed for"), T("Notes")});
    box(T("Drivers and privileges"), drivers_,
        T("StickyAim never installs drivers or changes security settings. Install third-party drivers yourself only after "
        "reading docs/DELIVERY.md."));
    launchers_ = makeTable({T("Launcher"), T("Installed"), T("Running"), T("Detected via"), T("Notes")});
    box(T("Launchers"), launchers_,
        T("The processing core is identical for every launcher. Differences are only in which controller the game ends up reading."));

    checklist_ = new QTableWidget(0, 4);
    checklist_->setHorizontalHeaderLabels({T("Launcher"), T("Check"), T("Result"), T("Notes (what you observed)")});
    checklist_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    checklist_->horizontalHeader()->setStretchLastSection(true);
    checklist_->verticalHeader()->setVisible(false);
    checklist_->setMinimumHeight(360);
    box(T("Manual compatibility checklist (your results)"), checklist_,
        T("These are filled in by you while testing. No result is pre-filled or assumed. Results are saved to your settings."));

    const auto& saved = app_->settings().value("compat_results", nlohmann::json::object());
    checklist_->setRowCount(static_cast<int>(std::size(kChecks)));
    for (int i = 0; i < static_cast<int>(std::size(kChecks)); ++i) {
        const auto& c = kChecks[i];
        checklist_->setItem(i, 0, new QTableWidgetItem(launcherName(c.launcher)));
        checklist_->item(i, 0)->setFlags(Qt::ItemIsEnabled);
        checklist_->setItem(i, 1, new QTableWidgetItem(T(c.test)));
        checklist_->item(i, 1)->setFlags(Qt::ItemIsEnabled);
        auto* combo = new QComboBox;
        for (const char* r : kResults) combo->addItem(T(r), QString::fromUtf8(r));
        std::string result = "Untested", notes;
        if (saved.contains(c.launcher) && saved[c.launcher].contains(c.test)) {
            result = saved[c.launcher][c.test].value("result", "Untested");
            notes = saved[c.launcher][c.test].value("notes", "");
        }
        combo->setCurrentIndex(std::max(0, combo->findData(QString::fromStdString(result))));
        checklist_->setCellWidget(i, 2, combo);
        checklist_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(notes)));
        connect(combo, &QComboBox::currentIndexChanged, this, &CompatPage::saveResults);
    }
    connect(checklist_, &QTableWidget::itemChanged, this, &CompatPage::saveResults);
    connect(rescanBtn, &QPushButton::clicked, this, &CompatPage::refresh);
    connect(exportBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, T("Export compatibility report"), app_->userDataDir() + "/compatibility_report.md",
                                                          T("Markdown (*.md)"));
        if (path.isEmpty()) return;
        std::string err;
        if (!sa::writeTextFile(path.toStdString(), buildReport().toStdString(), &err))
            QMessageBox::warning(this, T("Export"), QString::fromStdString(err));
    });
    refresh();
}

void CompatPage::saveResults() {
    nlohmann::json results = nlohmann::json::object();
    for (int i = 0; i < checklist_->rowCount(); ++i) {
        const auto& c = kChecks[i];
        auto* combo = qobject_cast<QComboBox*>(checklist_->cellWidget(i, 2));
        auto* notes = checklist_->item(i, 3);
        if (!combo || !notes) continue;
        results[c.launcher][c.test] = {{"result", combo->currentData().toString().toStdString()},
                                       {"notes", notes->text().toStdString()},
                                       {"updated", QDateTime::currentDateTime().toString(Qt::ISODate).toStdString()}};
    }
    app_->settings()["compat_results"] = results;
    app_->saveSettings();
}

void CompatPage::refresh() {
    const auto slots = sa::win::XInputSource::querySlots();
    xinput_->setRowCount(4);
    for (int i = 0; i < 4; ++i) {
        const auto& s = slots[static_cast<size_t>(i)];
        QString note;
        if (app_->source().kind == AppController::SourceKind::XInput && app_->source().slot == i) note = T("StickyAim input source");
        setRow(xinput_, i, {QString::number(i), s.connected ? T("yes") : T("no"), QString::fromStdString(s.subtype), s.connected ? (s.wireless ? T("yes") : T("no")) : "", note});
    }

    const auto hids = sa::win::enumerateHidDevices(true);
    hid_->setRowCount(static_cast<int>(hids.size()));
    for (int i = 0; i < static_cast<int>(hids.size()); ++i) {
        const auto& d = hids[static_cast<size_t>(i)];
        const sa::DeviceMode* mode = nullptr;
        const auto* k = sa::matchDevice(app_->deviceDb(), d.vendorId, d.productId, &mode);
        setRow(hid_, i,
               {QString::fromStdString(sa::hex4(d.vendorId) + ":" + sa::hex4(d.productId)),
                d.xinputHidInterface ? T("XInput HID view") : (d.interfaceNumber >= 0 ? QString(T("MI_%1")).arg(d.interfaceNumber, 2, 10, QChar('0')) : "-"),
                QString::fromStdString(d.usageName()), QString::fromStdString(d.product),
                k ? QString::fromStdString(k->name + " — " + mode->mode) : T("not in device database"),
                k ? (mode->verification.empty() ? T("unverified") : T("observed (see devices/*.json)")) : ""});
    }

    const auto drv = sa::win::probeDrivers();
    drivers_->setRowCount(5);
    setRow(drivers_, 0, {T("ViGEmBus driver"), QString::fromStdString(drv.vigemBus.state), T("Virtual controller output (optional)"),
                         T("Retired upstream (archived Nov 2023). Still functional; no further updates.")});
    setRow(drivers_, 1, {T("HidHide driver"), QString::fromStdString(drv.hidHide.state), T("Hiding the physical pad from games (optional)"),
                         T("Without it the game may see two controllers when the virtual pad is active.")});
    setRow(drivers_, 2, {"xusb22 (Microsoft Xbox 360 driver)", QString::fromStdString(drv.xusb.state), T("XInput controllers"), T("Part of Windows.")});
    setRow(drivers_, 3, {T("ViGEm client in this build"), sa::win::ViGEmX360Sink::isCompiledIn() ? T("compiled in") : T("not compiled"),
                         T("Virtual output"), T("Build option STICKYAIM_WITH_VIGEM.")});
    setRow(drivers_, 4, {T("Administrator"), drv.isElevated ? T("running elevated") : T("standard user"), T("Nothing in StickyAim"),
                         T("StickyAim does not require elevation. Driver installers do.")});

    const auto launchers = sa::win::probeLaunchers();
    launchers_->setRowCount(static_cast<int>(launchers.size()));
    for (int i = 0; i < static_cast<int>(launchers.size()); ++i) {
        const auto& l = launchers[static_cast<size_t>(i)];
        QStringList notes;
        for (const auto& n : l.notes) notes << QString::fromStdString(n);
        setRow(launchers_, i, {QString::fromStdString(l.name), l.installed ? T("yes") : T("no"), l.running ? T("yes") : T("no"),
                               QString::fromStdString(l.detection), notes.join(" ")});
        launchers_->item(i, 4)->setToolTip(notes.join("\n\n"));
    }

    summary_->setText(QString(T("Virtual output: <b>%1</b> · ViGEmBus %2 · HidHide %3 · %4 controller interface(s) found."))
                          .arg(app_->allowVirtualOutput() ? T("enabled in Settings") : T("disabled (safe preview only)"))
                          .arg(QString::fromStdString(drv.vigemBus.state))
                          .arg(QString::fromStdString(drv.hidHide.state))
                          .arg(hids.size()));
}

QString CompatPage::buildReport() const {
    auto tableToMd = [](QTableWidget* t) {
        QString md;
        QStringList head;
        for (int c = 0; c < t->columnCount(); ++c) head << t->horizontalHeaderItem(c)->text();
        md += "| " + head.join(" | ") + " |\n|" + QString(" --- |").repeated(t->columnCount()) + "\n";
        for (int r = 0; r < t->rowCount(); ++r) {
            QStringList row;
            for (int c = 0; c < t->columnCount(); ++c) {
                QString v;
                if (auto* combo = qobject_cast<QComboBox*>(t->cellWidget(r, c))) v = combo->currentText();
                else if (auto* it = t->item(r, c)) v = it->text();
                row << v.replace('|', '/').replace('\n', ' ');
            }
            md += "| " + row.join(" | ") + " |\n";
        }
        return md;
    };
    QString md = "# StickyAim Lab compatibility report\n\nGenerated " + QDateTime::currentDateTime().toString(Qt::ISODate) +
                 "\n\nAutomatically detected facts and user-recorded checklist results. Checklist entries marked Untested were not tested.\n\n";
    md += "## XInput slots\n\n" + tableToMd(xinput_);
    md += "\n## HID interfaces\n\n" + tableToMd(hid_);
    md += "\n## Drivers\n\n" + tableToMd(drivers_);
    md += "\n## Launchers\n\n" + tableToMd(launchers_);
    md += "\n## Manual checklist (user results)\n\n" + tableToMd(checklist_);
    return md;
}
