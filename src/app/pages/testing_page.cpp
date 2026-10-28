#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "stickyaim/core/config_io.hpp"
#include "widgets/grouped_list.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

// Identity profile: what the game would get from the raw controller.
sa::Profile rawProfile() {
    sa::Profile p;
    p.name = T("Raw controller (no processing)").toStdString();
    for (auto* c : {&p.hip, &p.ads}) {
        c->deadzone = {sa::DeadzoneType::None, 0.0f, 1.0f};
        c->circularSaturation = false;
    }
    p.moveStick = {sa::DeadzoneType::None, 0.0f, 1.0f};
    return p;
}

// A function, not a static: the language is chosen after static initialization.
QStringList columns() {
    return {T("Test"), T("Profile"), T("Added lag (ms)"), T("Gain ratio"), T("Rest output RMS"), T("Output levels"),
            T("Jerk RMS (smoothness)"), T("Saturated %")};
}

}  // namespace

TestingPage::TestingPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(22);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::heading(T("Testing")));
    auto* sub = ui::subheading(T("Compare the active profile with a baseline using repeatable signal measurements."));
    sub->setToolTip(T("Objective signal measurements comparing the active profile against a baseline. Synthetic tests use deterministic, "
        "seeded signals and are exactly repeatable. Live tests replay what you just did on the controller through both "
        "profiles. None of these numbers measure in-game aim performance or the game's aim assist."));
    titles->addWidget(sub);
    root->addLayout(titles);

    auto* lists = new QHBoxLayout;
    lists->setSpacing(18);
    auto* syn = new ui::Group(T("Synthetic comparison (repeatable)"));
    baseline_ = new QComboBox;
    syn->addRow(T("Baseline"), baseline_);
    context_ = new QComboBox;
    context_->addItems({T("Hip-fire"), T("ADS")});
    syn->addRow(T("Context"), context_);
    auto* runSyn = new QPushButton(T("Run"));
    runSyn->setObjectName("runSynthetic");
    runSyn->setProperty("primary", true);
    syn->addRow(T("Run synthetic suite"), runSyn, T("Same signals every time, so results are exactly repeatable."));
    lists->addWidget(syn, 1, Qt::AlignTop);

    auto* live = new ui::Group(T("Live capture (uses the active controller)"));
    liveKind_ = new QComboBox;
    liveKind_->addItems({T("Rest — hands off (3 s)"), T("Slow tracking — smooth circles (6 s)"), T("Micro-adjustments — tiny corrections (6 s)"),
                         T("Flicks — fast full moves (6 s)")});
    live->addRow(T("Movement"), liveKind_);
    liveBtn_ = new QPushButton(T("Start capture"));
    live->addRow(T("Record and replay through both profiles"), liveBtn_);
    liveStatus_ = live->footerLabel();
    liveStatus_->show();  // status messages are set directly on the label
    lists->addWidget(live, 1, Qt::AlignTop);
    root->addLayout(lists);

    auto* results = new ui::Group(T("Results"));
    const QStringList cols = columns();
    table_ = new QTableWidget(0, static_cast<int>(cols.size()));
    table_->setHorizontalHeaderLabels(cols);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setMinimumHeight(320);
    results->addContent(table_, 8);
    status_ = results->footerLabel();
    status_->show();
    root->addWidget(results);

    auto* subj = new ui::Group(T("Subjective notes (kept separate from measurements in the exported report)"));
    notes_ = new QPlainTextEdit;
    notes_->setPlaceholderText(T("e.g. blind A/B in a private match: which felt better for tracking, and why. Record test conditions."));
    notes_->setFixedHeight(90);
    subj->addContent(notes_, 12);
    auto* exportBtn = new QPushButton(T("Export…"));
    subj->addRow(T("Export report"), exportBtn, "JSON");
    root->addWidget(subj);
    root->addStretch();
    auto fillBaselines = [this] {
        baseline_->clear();
        baseline_->addItem(T("Raw controller (no processing)"), QString());
        for (const auto& p : app_->profileLibrary()) baseline_->addItem(QFileInfo(p).completeBaseName(), p);
    };
    fillBaselines();
    connect(app_, &AppController::profileChanged, this, fillBaselines);
    connect(runSyn, &QPushButton::clicked, this, &TestingPage::runSynthetic);
    connect(liveBtn_, &QPushButton::clicked, this, [this] { startLive(liveKind_->currentIndex() == 0 ? 3 : 6); });
    connect(exportBtn, &QPushButton::clicked, this, &TestingPage::exportReport);
}

void TestingPage::runSynthetic() {
    std::vector<std::string> errs;
    auto current = sa::CompiledProfile::compile(app_->profile(), {}, &errs);
    if (!current) {
        QMessageBox::warning(this, T("Testing"), T("Active profile is invalid: ") + QString::fromStdString(errs.front()));
        return;
    }
    sa::Profile base = rawProfile();
    const QString basePath = baseline_->currentData().toString();
    if (!basePath.isEmpty()) {
        std::string err;
        if (!sa::loadProfile(basePath.toStdString(), base, &err)) {
            QMessageBox::warning(this, T("Testing"), QString::fromStdString(err));
            return;
        }
    }
    auto baseline = sa::CompiledProfile::compile(base, {});
    const sa::ContextId ctx = context_->currentIndex() == 1 ? sa::ContextId::Ads : sa::ContextId::Hip;
    // Calibration is not applied to synthetic signals: they are already centered.
    auto a = sa::runSyntheticComparison(*current, ctx, 1000.0);
    auto b = sa::runSyntheticComparison(*baseline, ctx, 1000.0);
    showComparison({{T("Active: ") + QString::fromStdString(app_->profile().name), a}, {T("Baseline: ") + QString::fromStdString(base.name), b}});
    status_->setText(QString(T("Synthetic suite at 1000 Hz, context %1. Processing cost of active profile on this machine: %2 ns/sample (batch mean)."))
                         .arg(context_->currentText())
                         .arg(a.cost.batchMeanNs, 0, 'f', 1));
    lastReport_ = {{"generated", QDateTime::currentDateTime().toString(Qt::ISODate).toStdString()},
                   {"type", "synthetic"},
                   {"active", sa::toJson(a)},
                   {"baseline", sa::toJson(b)},
                   {"active_profile", sa::toJson(app_->profile())},
                   {"baseline_profile", sa::toJson(base)}};
}

void TestingPage::showComparison(const std::vector<std::pair<QString, sa::ComparisonReport>>& reps) {
    table_->setRowCount(0);
    if (reps.empty()) return;
    const size_t nTests = reps.front().second.tests.size();
    for (size_t t = 0; t < nTests; ++t) {
        for (const auto& [label, rep] : reps) {
            if (t >= rep.tests.size()) continue;
            const auto& r = rep.tests[t];
            const int row = table_->rowCount();
            table_->insertRow(row);
            const QStringList cells = {T((r.name).c_str()),
                                       label,
                                       ui::fmt(r.addedLagMs, 2),
                                       ui::fmt(r.gainRatio, 3),
                                       ui::fmt(r.procRestRms, 4),
                                       QString::number(r.procDistinctLevels),
                                       QString::number(r.procJerkRms, 'e', 2),
                                       ui::fmt(r.procSaturatedFraction * 100, 1)};
            for (int c = 0; c < cells.size(); ++c) {
                auto* item = new QTableWidgetItem(cells[c]);
                item->setToolTip(T((r.description).c_str()));
                table_->setItem(row, c, item);
            }
        }
    }
}

void TestingPage::startLive(int seconds) {
    if (!app_->engineRunning() || app_->source().kind == AppController::SourceKind::Demo) {
        liveStatus_->setText(T("<span style='color:#FF453A'>Select your controller as input on the Dashboard and start the engine.</span>"));
        return;
    }
    liveName_ = liveKind_->currentText().section(" —", 0, 0).toLower().replace(' ', '_');
    liveBtn_->setEnabled(false);
    app_->beginCapture();
    liveStatus_->setText(T("Capturing… ") + liveKind_->currentText());
    QTimer::singleShot(seconds * 1000, this, &TestingPage::finishLive);
}

void TestingPage::finishLive() {
    const auto rec = app_->endCapture();
    liveBtn_->setEnabled(true);
    if (rec.size() < 50) {
        liveStatus_->setText(QString(T("Only %1 samples captured — nothing to analyze.")).arg(rec.size()));
        return;
    }
    auto current = sa::CompiledProfile::compile(app_->profile(), app_->calibration());
    sa::Profile base = rawProfile();
    const QString basePath = baseline_->currentData().toString();
    if (!basePath.isEmpty()) sa::loadProfile(basePath.toStdString(), base);
    auto baseline = sa::CompiledProfile::compile(base, app_->calibration());
    if (!current || !baseline) return;
    sa::ComparisonReport a, b;
    a.tests.push_back(sa::analyzeRecording(*current, rec, "live_" + liveName_.toStdString()));
    b.tests.push_back(sa::analyzeRecording(*baseline, rec, "live_" + liveName_.toStdString()));
    showComparison({{T("Active: ") + QString::fromStdString(app_->profile().name), a}, {T("Baseline: ") + QString::fromStdString(base.name), b}});
    const double dur = (rec.back().t - rec.front().t) * 1e-9;
    liveStatus_->setText(QString(T("%1 samples over %2 s (%3/s), replayed through both profiles.")).arg(rec.size()).arg(dur, 0, 'f', 1).arg(rec.size() / dur, 0, 'f', 0));
    lastReport_ = {{"generated", QDateTime::currentDateTime().toString(Qt::ISODate).toStdString()},
                   {"type", "live_capture"},
                   {"capture", liveName_.toStdString()},
                   {"samples", rec.size()},
                   {"duration_s", dur},
                   {"active", sa::toJson(a.tests.front())},
                   {"baseline", sa::toJson(b.tests.front())},
                   {"active_profile", sa::toJson(app_->profile())},
                   {"baseline_profile", sa::toJson(base)}};
}

void TestingPage::exportReport() {
    if (lastReport_.is_null()) {
        QMessageBox::information(this, T("Export"), T("Run a test first."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, T("Export report"), app_->userDataDir() + "/comparison_report.json", T("JSON (*.json)"));
    if (path.isEmpty()) return;
    nlohmann::json out = lastReport_;
    out["objective_measurements_note"] = T("Signal-level measurements only; not a measure of in-game performance or aim assist.").toStdString();
    out["subjective_notes"] = notes_->toPlainText().toStdString();
    std::string err;
    if (!sa::writeTextFile(path.toStdString(), out.dump(2) + "\n", &err)) QMessageBox::warning(this, T("Export"), QString::fromStdString(err));
}
