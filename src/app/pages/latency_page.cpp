#include <QGridLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "stickyaim/platform/rawinput.hpp"
#include "widgets/histogram_view.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

LatencyPage::LatencyPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* root = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    top->addWidget(ui::heading(T("Latency Lab")));
    top->addStretch();
    auto* reset = new QPushButton(T("Reset statistics"));
    top->addWidget(reset);
    root->addLayout(top);
    root->addWidget(ui::note(
        T("All values are measured on this machine with host-side QPC timestamps. 'Sample interval' is the time between new states "
        "seen by the engine (for XInput: state changes, which can be fewer than USB reports). 'Software path' covers acquisition → "
        "processing → delivery inside this process only; USB transfer, driver, game and display latency are NOT included. USB polling "
        "rate is not the same as this application's effective processing rate.")));

    auto* grid = new QGridLayout;
    intervals_ = new HistogramView(T("Engine sample interval (new states)"), "µs");
    process_ = new HistogramView(T("Processing time per sample (Processor::process)"), "µs");
    total_ = new HistogramView(T("Software path: acquisition → submit complete"), "µs");
    grid->addWidget(intervals_, 0, 0);
    grid->addWidget(process_, 0, 1);
    grid->addWidget(total_, 1, 0);

    auto* statsBox = new QGroupBox(T("Measured summary"));
    auto* statsL = new QVBoxLayout(statsBox);
    summary_ = new QLabel;
    summary_->setWordWrap(true);
    summary_->setTextFormat(Qt::RichText);
    summary_->setMinimumHeight(140);
    engineStats_ = new QLabel;
    engineStats_->setWordWrap(true);
    statsL->addWidget(summary_);
    statsL->addWidget(engineStats_);
    statsL->addStretch();
    grid->addWidget(statsBox, 1, 1);
    root->addLayout(grid, 2);

    auto* hidBox = new QGroupBox(T("Raw Input HID report timing per device (arrival at this process)"));
    auto* hidL = new QVBoxLayout(hidBox);
    hid_ = new QTableWidget(0, 9);
    hid_->setHorizontalHeaderLabels({T("Device"), T("Interface"), T("Reports"), T("Rate (Hz)"), T("Mean µs"), T("SD µs"), "p99 µs", T("Max µs"), T("Multi-report msgs")});
    hid_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    hid_->horizontalHeader()->setStretchLastSection(true);
    hid_->verticalHeader()->setVisible(false);
    hid_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hidL->addWidget(hid_);
    hidL->addWidget(ui::note(T("Move the stick continuously while measuring. Some controllers only send reports when something changes.")));
    root->addWidget(hidBox, 1);

    auto* cfgBox = new QGroupBox(T("Engine polling"));
    auto* cfgL = new QHBoxLayout(cfgBox);
    mode_ = new QComboBox;
    mode_->addItems({T("Spin (lowest latency, one core busy)"), T("Hybrid (timer + short spin)"), T("Timer only (lowest CPU)")});
    interval_ = new QSpinBox;
    interval_->setRange(50, 20000);
    interval_->setSuffix(" µs");
    mmcss_ = new QCheckBox(T("MMCSS 'Games' priority"));
    auto* apply = new QPushButton(T("Apply (restarts engine)"));
    cfgL->addWidget(new QLabel(T("Mode")));
    cfgL->addWidget(mode_);
    cfgL->addWidget(new QLabel(T("Interval")));
    cfgL->addWidget(interval_);
    cfgL->addWidget(mmcss_);
    cfgL->addWidget(apply);
    cfgL->addStretch();
    root->addWidget(cfgBox);

    const auto cfg = app_->engineConfig();
    mode_->setCurrentIndex(static_cast<int>(cfg.mode));
    interval_->setValue(cfg.intervalUs);
    mmcss_->setChecked(cfg.useMmcss);

    connect(reset, &QPushButton::clicked, this, [this] { app_->resetLatencyStats(); });
    connect(apply, &QPushButton::clicked, this, [this] {
        auto c = app_->engineConfig();
        c.mode = static_cast<sa::PollMode>(mode_->currentIndex());
        c.intervalUs = interval_->value();
        c.useMmcss = mmcss_->isChecked();
        app_->setEngineConfig(c);
    });
    auto* timer = new QTimer(this);
    timer->setInterval(250);
    connect(timer, &QTimer::timeout, this, &LatencyPage::refresh);
    timer->start();
}

void LatencyPage::refresh() {
    if (!isVisible()) return;
    intervals_->setHistogram(app_->sampleTiming().intervals());
    process_->setHistogram(app_->processHist());
    total_->setHistogram(app_->totalHist());
    const auto t = app_->sampleTiming().summary();
    const auto& sub = app_->submitHist();
    summary_->setText(QString(T("<b>Sample interval</b>: mean %1 µs (≈ %2 new states/s), jitter SD %3 µs, p50 %4, p99 %5, max %6 µs, "
                              "gaps %7<br><b>Processing</b>: p50 %8 µs, p99 %9 µs<br><b>Delivery (sink submit)</b>: p50 %10 µs, p99 %11 µs"
                              "<br><b>Software path</b>: p50 %12 µs, p99 %13 µs, max %14 µs<br>"
                              "<span style='color:#98989D'>Per-sample times are quantized by the timer (QPC, typically 0.1 µs).</span>"))
                          .arg(ui::fmt(t.meanUs, 1))
                          .arg(ui::fmt(t.effectiveRateHz, 0))
                          .arg(ui::fmt(t.stddevUs, 1))
                          .arg(ui::fmt(t.p50Us, 1))
                          .arg(ui::fmt(t.p99Us, 1))
                          .arg(ui::fmt(t.maxUs, 1))
                          .arg(t.gaps)
                          .arg(ui::fmt(app_->processHist().percentile(50), 2))
                          .arg(ui::fmt(app_->processHist().percentile(99), 2))
                          .arg(ui::fmt(sub.percentile(50), 1))
                          .arg(ui::fmt(sub.percentile(99), 1))
                          .arg(ui::fmt(app_->totalHist().percentile(50), 1))
                          .arg(ui::fmt(app_->totalHist().percentile(99), 1))
                          .arg(ui::fmt(app_->totalHist().max(), 1)));
    const auto c = app_->counters();
    engineStats_->setText(QString(T("Engine: %1 · mode %2 / %3 µs · missed device packets %4 · late wake-ups %5 (max %6 µs) · "
                                  "telemetry drops %7 · sink errors %8"))
                              .arg(T(sa::toString(c.state)))
                              .arg(sa::toString(app_->engineConfig().mode))
                              .arg(app_->engineConfig().intervalUs)
                              .arg(c.missedSequence)
                              .arg(c.lateWakeups)
                              .arg(c.maxLateUs)
                              .arg(c.telemetryDrops)
                              .arg(c.sinkErrors));

    const auto snap = app_->rawInput() ? app_->rawInput()->snapshot() : std::vector<sa::win::HidReportStats>{};
    hid_->setRowCount(static_cast<int>(snap.size()));
    for (int i = 0; i < static_cast<int>(snap.size()); ++i) {
        const auto& d = snap[static_cast<size_t>(i)];
        const QStringList cells = {
            QString("%1 (%2:%3)").arg(QString::fromStdString(d.info.product), QString::fromStdString(sa::hex4(d.info.vendorId)),
                                      QString::fromStdString(sa::hex4(d.info.productId))),
            T((d.info.usageName()).c_str()) + (d.info.xinputHidInterface ? T(" (XInput HID view)") : ""),
            QString::number(d.reports),
            ui::fmt(d.timing.effectiveRateHz, 0),
            ui::fmt(d.timing.meanUs, 1),
            ui::fmt(d.timing.stddevUs, 1),
            ui::fmt(d.timing.p99Us, 1),
            ui::fmt(d.timing.maxUs, 1),
            QString::number(d.coalescedMessages)};
        for (int c2 = 0; c2 < cells.size(); ++c2) hid_->setItem(i, c2, new QTableWidgetItem(cells[c2]));
    }
}
