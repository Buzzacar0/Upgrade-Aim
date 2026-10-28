#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "i18n.hpp"
#include "pages/pages.hpp"
#include "stickyaim/platform/rawinput.hpp"
#include "stickyaim/platform/xinput_source.hpp"
#include "widgets/stick_view.hpp"
#include "widgets/ui_helpers.hpp"

namespace {

QLocale uiLocale() {
    return i18n::language() == i18n::Lang::En ? QLocale(QLocale::English, QLocale::UnitedStates)
                                              : QLocale(QLocale::Portuguese, QLocale::Brazil);
}

void setTextIfChanged(QLabel* l, const QString& text) {
    if (l->text() != text) l->setText(text);  // avoids needless relayouts at 60+ fps
}

}  // namespace

DashboardPage::DashboardPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(18);

    // ---- header: title, detected controller, primary actions
    auto* top = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::heading(T("Dashboard")));
    device_ = ui::subheading({});
    titles->addWidget(device_);
    top->addLayout(titles, 1);
    // Active / Bypass switch, as in Upgrade Sound: on = the profile processes
    // the sticks, off = the controller state goes through unchanged.
    active_ = new QCheckBox;
    active_->setObjectName("bigSwitch");
    active_->setCursor(Qt::PointingHandCursor);
    active_->setToolTip(T("Send the raw controller state unchanged (Ctrl+Alt+P, or hold Back+Start)."));
    activeLabel_ = new QLabel;
    activeLabel_->setStyleSheet("font-size: 14px; font-weight: 600;");
    startStop_ = new QPushButton;
    startStop_->setProperty("primary", true);
    startStop_->setMinimumWidth(130);
    top->addWidget(activeLabel_, 0, Qt::AlignVCenter);
    top->addSpacing(10);
    top->addWidget(active_, 0, Qt::AlignVCenter);
    top->addSpacing(18);
    top->addWidget(startStop_, 0, Qt::AlignVCenter);
    root->addLayout(top);

    // ---- metric cards
    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(14);
    const QString titlesText[4] = {T("Controller read rate"), T("Without the app"), T("With the app"), T("Smoothing")};
    const QString tips[4] = {
        T("State changes the controller delivered per second, counted from its packet counter. A monitor cannot show 8 kHz; the "
          "engine still reads every state."),
        T("Time between new controller states, which the game sees directly. USB, driver and in-game time are the same either way "
          "and cannot be measured here."),
        T("Controller interval plus what the app adds: waiting for the next read, processing and delivery, plus the profile's "
          "smoothing. Measured live; the virtual controller → game hop is not measured."),
        T("Lag added by the active profile's smoothing, measured with the synthetic 1 Hz tracking test. Deadzones and curves add "
          "0 ms."),
    };
    for (int i = 0; i < 4; ++i) {
        QVBoxLayout* l = nullptr;
        auto* c = ui::card(&l);
        c->setToolTip(tips[i]);
        l->addWidget(ui::caption(titlesText[i]));
        auto* valueRow = new QHBoxLayout;
        valueRow->setSpacing(6);
        tileValue_[i] = new QLabel("—");
        tileValue_[i]->setObjectName("metric");
        tileUnit_[i] = new QLabel;
        tileUnit_[i]->setObjectName("metricUnit");
        valueRow->addWidget(tileValue_[i], 0, Qt::AlignBaseline);
        valueRow->addWidget(tileUnit_[i], 0, Qt::AlignBaseline);
        valueRow->addStretch();
        l->addLayout(valueRow);
        tileSub_[i] = ui::note({});
        l->addWidget(tileSub_[i]);
        metrics->addWidget(c, 1);
    }
    root->addLayout(metrics);

    // ---- sticks
    auto* sticks = new QHBoxLayout;
    sticks->setSpacing(14);
    // Controller order: left stick on the left, right stick on the right.
    left_ = new StickView(T("Movement · left stick"));
    right_ = new StickView(T("Aim · right stick"));
    sticks->addWidget(left_, 1);
    sticks->addWidget(right_, 1);
    root->addLayout(sticks, 1);

    // ---- controller, profile, engine
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(14);

    QVBoxLayout* devL = nullptr;
    auto* devCard = ui::card(&devL);
    devL->addWidget(ui::caption(T("Input source")));
    auto* srcRow = new QHBoxLayout;
    srcRow->setSpacing(8);
    sourceCombo_ = new QComboBox;
    auto* rescan = new QPushButton(T("Rescan"));
    rescan->setToolTip(T("Rescan devices"));
    srcRow->addWidget(sourceCombo_, 1);
    srcRow->addWidget(rescan);
    devL->addLayout(srcRow);
    source_ = ui::note({});
    drift_ = ui::note({});
    devL->addWidget(source_);
    devL->addWidget(drift_);
    devL->addStretch();
    bottom->addWidget(devCard, 1);

    QVBoxLayout* profL = nullptr;
    auto* profCard = ui::card(&profL);
    profL->addWidget(ui::caption(T("Active profile")));
    profileCombo_ = new QComboBox;
    profileCombo_->setToolTip(T("Ctrl+Alt+1…9 switches to the n-th profile in this list instantly."));
    profL->addWidget(profileCombo_);
    profile_ = ui::note({});
    profile_->setMaximumHeight(36);
    context_ = new QLabel;
    profL->addWidget(profile_);
    profL->addWidget(context_);
    profL->addStretch();
    bottom->addWidget(profCard, 1);

    // The one number most players change in the game: the app compensates it
    // so small movements still reach the game and trigger its aim assist.
    QVBoxLayout* gameL = nullptr;
    auto* gameCard = ui::card(&gameL);
    gameL->addWidget(ui::caption(T("Game deadzone (right stick)")));
    auto* gameDz = new QSpinBox;
    gameDz->setRange(0, 40);
    gameDz->setKeyboardTracking(false);
    gameDz->setFixedWidth(110);
    gameDz->setValue(app_->gameDeadzonePercent());
    gameL->addWidget(gameDz);
    gameL->addWidget(ui::note(T("Type the same value you use in the game. The app compensates it so your small "
                                "corrections still reach the game and keep its aim assist engaged.")));
    gameL->addStretch();
    connect(gameDz, &QSpinBox::valueChanged, this, [this](int v) { app_->setGameDeadzonePercent(v); });
    bottom->addWidget(gameCard, 1);

    QVBoxLayout* engL = nullptr;
    auto* engCard = ui::card(&engL);
    engL->addWidget(ui::caption(T("Engine")));
    engine_ = new QLabel;
    rate_ = ui::note({});
    counters_ = ui::note({});
    counters_->setVisible(false);
    auto* details = new QPushButton(T("Technical details"));
    details->setCheckable(true);
    details->setStyleSheet("QPushButton { background: transparent; color: #B44DFF; padding: 2px 0; text-align: left; }"
                           "QPushButton:checked { background: transparent; color: #C77DFF; }");
    details->setCursor(Qt::PointingHandCursor);
    engL->addWidget(engine_);
    engL->addWidget(rate_);
    engL->addWidget(details, 0, Qt::AlignLeft);
    engL->addWidget(counters_);
    engL->addStretch();
    bottom->addWidget(engCard, 1);
    root->addLayout(bottom);

    badge_ = nullptr;  // the mode pill lives in the sidebar

    connect(details, &QPushButton::toggled, counters_, &QWidget::setVisible);
    connect(startStop_, &QPushButton::clicked, this, [this] {
        if (app_->engineRunning()) {
            app_->stopEngine();
        } else {
            QString err;
            if (!app_->startEngine(&err)) QMessageBox::warning(this, T("Engine"), err);
        }
        refresh();
    });
    connect(active_, &QCheckBox::toggled, this, [this](bool on) { app_->setBypass(!on); });
    connect(sourceCombo_, &QComboBox::activated, this, [this](int idx) {
        const QVariantList v = sourceCombo_->itemData(idx).toList();
        AppController::SourceSel sel;
        sel.kind = static_cast<AppController::SourceKind>(v.value(0).toInt());
        sel.slot = v.value(1).toInt();
        sel.hidPath = v.value(2).toString();
        sel.label = sourceCombo_->itemText(idx);
        app_->setSource(sel);
        if (!app_->engineRunning()) app_->startEngine(nullptr);
    });
    connect(profileCombo_, &QComboBox::activated, this, [this](int idx) {
        QString err;
        if (!app_->loadProfileFile(profileCombo_->itemData(idx).toString(), &err)) QMessageBox::warning(this, T("Profile"), err);
    });

    connect(app_, &AppController::telemetryTick, this, &DashboardPage::refresh);
    connect(app_, &AppController::engineStateChanged, this, &DashboardPage::refresh);
    connect(app_, &AppController::profileChanged, this, &DashboardPage::refreshProfiles);

    // Populate sources.
    auto populate = [this] {
        sourceCombo_->clear();
        for (const auto& s : sa::win::XInputSource::querySlots())
            if (s.connected)
                sourceCombo_->addItem(QString(T("XInput slot %1 (%2)")).arg(s.slot).arg(T((s.subtype).c_str())),
                                      QVariantList{0, s.slot, QString()});
        for (const auto& d : sa::win::enumerateHidDevices(true)) {
            if (!(d.usagePage == 0x01 && (d.usage == 0x04 || d.usage == 0x05))) continue;
            sourceCombo_->addItem(QString(T("HID %1 %2%3"))
                                      .arg(QString::fromStdString(d.product))
                                      .arg(QString::fromStdString(sa::hex4(d.vendorId) + ":" + sa::hex4(d.productId)))
                                      .arg(d.xinputHidInterface ? T(" (XInput HID view)") : ""),
                                  QVariantList{1, 0, QString::fromStdString(d.path)});
        }
        sourceCombo_->addItem(T("Demo signal (no hardware, preview only)"), QVariantList{2, 0, QString()});
        const auto cur = app_->source();
        for (int i = 0; i < sourceCombo_->count(); ++i) {
            const auto v = sourceCombo_->itemData(i).toList();
            if (v.value(0).toInt() == static_cast<int>(cur.kind) &&
                (cur.kind != AppController::SourceKind::XInput || v.value(1).toInt() == cur.slot) &&
                (cur.kind != AppController::SourceKind::Hid || v.value(2).toString() == cur.hidPath))
                sourceCombo_->setCurrentIndex(i);
        }
    };
    populate();
    connect(rescan, &QPushButton::clicked, this, [this, populate] {
        populate();
        app_->refreshDeviceMatch();
        refresh();
    });

    refreshProfiles();
    refresh();
}

void DashboardPage::refreshProfiles() {
    profileCombo_->blockSignals(true);
    profileCombo_->clear();
    int current = -1;
    for (const auto& path : app_->profileLibrary()) {
        profileCombo_->addItem(QFileInfo(path).completeBaseName(), path);
        if (QFileInfo(path) == QFileInfo(app_->profilePath())) current = profileCombo_->count() - 1;
    }
    if (current < 0) {
        profileCombo_->addItem(QString::fromStdString(app_->profile().name) + T(" (unsaved)"), QString());
        current = profileCombo_->count() - 1;
    }
    profileCombo_->setCurrentIndex(current);
    profileCombo_->blockSignals(false);
    const auto& p = app_->profile();
    QString text = QString::fromStdString(p.name).toHtmlEscaped();
    if (app_->profileDirty()) text += T(" — modified");
    profile_->setText(text);
    profile_->setToolTip(QString::fromStdString(p.description));
    if (!app_->profileErrors().isEmpty())
        profile_->setText(text + "<br><span style='color:#FF453A'>" + app_->profileErrors().join("<br>") + "</span>");
}

void DashboardPage::refreshResponse(bool running, bool bypass, bool /*injecting*/) {
    const auto& r = app_->responseStats();
    if (!running || r.deviceHz <= 0) {
        for (int i = 0; i < 4; ++i) {
            setTextIfChanged(tileValue_[i], "—");
            setTextIfChanged(tileUnit_[i], {});
            setTextIfChanged(tileSub_[i], i == 0 ? (running ? T("Waiting for data from the controller…") : T("Engine stopped.")) : QString());
        }
        return;
    }
    const QLocale loc = uiLocale();
    auto ms3 = [&](double v) { return loc.toString(v, 'f', 3); };

    setTextIfChanged(tileValue_[0], loc.toString(qRound64(r.deviceHz)));
    setTextIfChanged(tileUnit_[0], "Hz");
    setTextIfChanged(tileSub_[0], QString(T("App read %1 % · screen %2 fps"))
                                      .arg(loc.toString(std::min(100.0, 100.0 * r.appHz / r.deviceHz), 'f', 0))
                                      .arg(loc.toString(r.uiFps, 'f', 0)) +
                                      (r.deviceRateValid ? QString() : T(" (no device counter: app rate shown)")));

    setTextIfChanged(tileValue_[1], ms3(r.deviceIntervalMs));
    setTextIfChanged(tileUnit_[1], "ms");
    setTextIfChanged(tileSub_[1], T("Between controller states"));

    const bool ads = app_->hasLatest() && app_->latest().context == static_cast<std::uint8_t>(sa::ContextId::Ads);
    const double smoothing = bypass ? 0.0 : (ads ? r.smoothingAdsMs : r.smoothingHipMs);
    setTextIfChanged(tileValue_[2], ms3(r.deviceIntervalMs + r.addedMeanMs + smoothing));
    setTextIfChanged(tileUnit_[2], "ms");
    QString with = QString(T("+%1 ms from the app · worst %2 ms")).arg(ms3(r.addedMeanMs)).arg(ms3(r.addedP99Ms));
    if (smoothing > 0) with += QString(T(" · +%1 ms smoothing")).arg(loc.toString(smoothing, 'f', 0));
    setTextIfChanged(tileSub_[2], with);

    setTextIfChanged(tileValue_[3], bypass ? T("off") : "+" + loc.toString(smoothing, 'f', 0));
    setTextIfChanged(tileUnit_[3], bypass ? QString() : "ms");
    setTextIfChanged(tileSub_[3], QString(T("Hip %1 ms · ADS %2 ms"))
                                      .arg(loc.toString(r.smoothingHipMs, 'f', 0))
                                      .arg(loc.toString(r.smoothingAdsMs, 'f', 0)));
}

void DashboardPage::refresh() {
    const auto c = app_->counters();
    const bool running = app_->engineRunning();
    const bool injecting = running && app_->output() == AppController::OutputKind::ViGEm;

    const QString start = running ? T("Stop engine") : T("Start engine");
    if (startStop_->text() != start) startStop_->setText(start);
    active_->blockSignals(true);
    active_->setChecked(!c.bypass);
    active_->blockSignals(false);
    active_->setEnabled(running);
    const QString activeText = !running ? T("Engine stopped.")
                               : c.bypass ? T("Bypass · original controller")
                                          : T("Active · processed by Upgrade Aim");
    if (activeLabel_->text() != activeText) {
        activeLabel_->setText(activeText);
        activeLabel_->setStyleSheet(QString("font-size: 14px; font-weight: 600; color: %1;")
                                        .arg(!running ? "#8E8E93" : c.bypass ? "#FF9F0A" : "#30D158"));
    }

    const QString matched = app_->matchedDeviceName();
    setTextIfChanged(device_, (matched.isEmpty() ? T("No known controller detected") : matched) + "  ·  " + app_->sourceDescription());
    setTextIfChanged(source_, T("Reading: ") + app_->sourceDescription());
    setTextIfChanged(engine_, QString(T("State: <b>%1</b> &nbsp; Output: %2"))
                                  .arg(T(sa::toString(c.state)))
                                  .arg(running ? (injecting ? T("ViGEm") : T("Preview")) : "—"));
    setTextIfChanged(rate_, QString(T("%1 new samples/s")).arg(uiLocale().toString(app_->newSampleRateHz(), 'f', 0)));
    if (counters_->isVisible())
        setTextIfChanged(counters_, QString(T("loops %1 · new %2 · idle ticks %3 · missed packets %4 · late wake-ups %5 (max %6 µs) · "
                                              "disconnects %7 · profile swaps %8<br>priority: %9"))
                                        .arg(c.loops)
                                        .arg(c.newSamples)
                                        .arg(c.idleTicks)
                                        .arg(c.missedSequence)
                                        .arg(c.lateWakeups)
                                        .arg(c.maxLateUs)
                                        .arg(c.disconnects)
                                        .arg(c.profileSwaps)
                                        .arg(T((c.priority).c_str())));

    refreshResponse(running, c.bypass, injecting);

    if (!app_->hasLatest()) return;
    const auto& recent = app_->recent();
    const size_t n = std::min<size_t>(recent.size(), 400);
    const size_t step = std::max<size_t>(1, n / 200);
    std::vector<sa::Vec2> lraw, lout, rraw, rout;
    for (size_t i = recent.size() - n; i < recent.size(); i += step) {
        const auto& t = recent[i];
        lraw.push_back({sa::normalizeAxis(t.raw.lx), sa::normalizeAxis(t.raw.ly)});
        lout.push_back({sa::normalizeAxis(t.out.lx), sa::normalizeAxis(t.out.ly)});
        rraw.push_back({sa::normalizeAxis(t.raw.rx), sa::normalizeAxis(t.raw.ry)});
        rout.push_back({sa::normalizeAxis(t.out.rx), sa::normalizeAxis(t.out.ry)});
    }
    const auto& last = app_->latest();
    rraw.push_back({sa::normalizeAxis(last.raw.rx), sa::normalizeAxis(last.raw.ry)});
    rout.push_back({sa::normalizeAxis(last.out.rx), sa::normalizeAxis(last.out.ry)});
    lraw.push_back({sa::normalizeAxis(last.raw.lx), sa::normalizeAxis(last.raw.ly)});
    lout.push_back({sa::normalizeAxis(last.out.lx), sa::normalizeAxis(last.out.ly)});
    left_->setPoints(lraw, lout);
    const bool ads = last.context == static_cast<std::uint8_t>(sa::ContextId::Ads);
    right_->setProcessedColor(ads ? ui::Colors::ads() : ui::Colors::hip());
    right_->setPoints(rraw, rout);
    const auto& ctx = app_->profile().context(ads ? sa::ContextId::Ads : sa::ContextId::Hip);
    right_->setRings(ctx.deadzone.inner, ctx.deadzone.outer);
    left_->setRings(app_->profile().moveStick.inner, app_->profile().moveStick.outer);
    setTextIfChanged(context_, QString(T("Aim context: <b style='color:%1'>%2</b> (LT %3)"))
                                   .arg(ads ? ui::Colors::ads().name() : ui::Colors::hip().name())
                                   .arg(ads ? T("ADS") : T("Hip-fire"))
                                   .arg(uiLocale().toString(sa::normalizeTrigger(last.raw.leftTrigger), 'f', 2)));
    const auto& cal = app_->calibration();
    setTextIfChanged(drift_, QString(T("Center calibration: (%1, %2)"))
                                 .arg(uiLocale().toString(cal.right.centerX, 'f', 4))
                                 .arg(uiLocale().toString(cal.right.centerY, 'f', 4)));
}
