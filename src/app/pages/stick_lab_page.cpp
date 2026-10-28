#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "widgets/context_editor.hpp"
#include "widgets/curve_editor.hpp"
#include "widgets/stick_view.hpp"
#include "widgets/time_plot.hpp"
#include "widgets/grouped_list.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

sa::ResponseShape& selectedShape(sa::AimContextConfig& c, int axis) {
    if (c.curveMode == sa::CurveMode::Radial) return c.radial;
    return axis == 2 ? c.y : c.x;
}

}  // namespace

StickLabPage::StickLabPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(18);
    auto* top = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::heading(T("Stick Lab")));
    titles->addWidget(ui::subheading(T("See each stick live, shape the response curve and calibrate your controller.")));
    top->addLayout(titles, 1);
    contextLabel_ = new QLabel;
    top->addWidget(contextLabel_, 0, Qt::AlignTop);
    root->addLayout(top);

    // Live views (left) and the response graph (right).
    auto* body = new QHBoxLayout;
    body->setSpacing(18);
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(14);
    auto* views = new QHBoxLayout;
    views->setSpacing(14);
    // One view per stick, in controller order; each overlays raw and processed.
    leftView_ = new StickView(T("Movement · left stick"));
    rightView_ = new StickView(T("Aim · right stick"));
    views->addWidget(leftView_);
    views->addWidget(rightView_);
    leftCol->addLayout(views, 3);
    plot_ = new TimePlot(T("Right stick X / Y over time"), -1, 1);
    plot_->setMinimumHeight(200);
    leftCol->addWidget(plot_, 2);
    body->addLayout(leftCol, 1);

    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(12);
    auto* sel = new QHBoxLayout;
    sel->setSpacing(10);
    view_ = new ui::Segmented({T("Full response"), T("Curve shape")});
    editContext_ = new ui::Segmented({T("Hip-fire"), T("ADS")});
    editAxis_ = new QComboBox;
    editAxis_->addItems({T("Magnitude"), T("X axis"), T("Y axis")});
    editAxis_->setMinimumWidth(120);
    sel->addWidget(view_);
    sel->addStretch();
    sel->addWidget(editContext_);
    sel->addWidget(editAxis_);
    rightCol->addLayout(sel);
    curve_ = new CurveEditor;
    curve_->setMinimumHeight(420);
    rightCol->addWidget(curve_, 1);
    body->addLayout(rightCol, 1);
    root->addLayout(body, 1);

    // Calibration (left) and the selected shape (right), as grouped lists.
    auto* lists = new QHBoxLayout;
    lists->setSpacing(18);
    lists->setAlignment(Qt::AlignTop);
    auto* cal = new ui::Group(T("Calibration"));
    restBtn_ = new QPushButton(T("Start"));
    cal->addRow(T("Rest test"), restBtn_, T("3 s without touching the controller. Measures center and noise."));
    rangeBtn_ = new QPushButton(T("Start"));
    cal->addRow(T("Range sweep"), rangeBtn_, T("6 s rotating the stick against the rim."));
    calStatus_ = new QLabel(T("Run the rest test first. Results are measurements of this controller only."));
    calStatus_->setWordWrap(true);
    calStatus_->setTextFormat(Qt::RichText);
    calStatus_->setObjectName("rowSubtitle");
    cal->addContent(calStatus_);
    applyCenter_ = new QPushButton(T("Apply"));
    cal->addRow(T("Measured center"), applyCenter_);
    applyDeadzone_ = new QPushButton(T("Apply"));
    cal->addRow(T("Recommended deadzone"), applyDeadzone_, T("Applied to hip-fire and ADS."));
    applyRange_ = new QPushButton(T("Apply"));
    cal->addRow(T("Range and outer threshold"), applyRange_);
    for (auto* b : {applyCenter_, applyDeadzone_, applyRange_}) b->setEnabled(false);
    lists->addWidget(cal, 1, Qt::AlignTop);

    auto* shapeCol = new QVBoxLayout;
    shapeCol->setSpacing(22);
    auto* dz = new ui::Group(T("Selected shape"));
    inner_ = ui::spin(0.0, 0.85, 0.005, 3);
    dz->addRow(T("Inner deadzone"), inner_);
    outer_ = ui::spin(0.15, 1.0, 0.005, 3);
    dz->addRow(T("Outer threshold"), outer_);
    shapeCol->addWidget(dz);
    shapeEdit_ = new ShapeEditor(T("Curve"));
    shapeCol->addWidget(shapeEdit_);
    shapeCol->addStretch();
    lists->addLayout(shapeCol, 1);
    root->addLayout(lists);
    auto pushEdit = [this] {
        if (updating_) return;
        sa::Profile p = app_->profile();
        auto& ctx = p.context(editContext_->current() == 1 ? sa::ContextId::Ads : sa::ContextId::Hip);
        ctx.deadzone.inner = static_cast<float>(inner_->value());
        ctx.deadzone.outer = static_cast<float>(outer_->value());
        auto& shape = selectedShape(ctx, editAxis_->currentIndex());
        const auto points = shape.curve.points;
        shape = shapeEdit_->shape();
        if (shape.curve.points.empty()) shape.curve.points = points;
        app_->setProfile(p);
    };
    connect(shapeEdit_, &ShapeEditor::changed, this, pushEdit);
    connect(inner_, &QDoubleSpinBox::valueChanged, this, pushEdit);
    connect(outer_, &QDoubleSpinBox::valueChanged, this, pushEdit);
    connect(curve_, &CurveEditor::pointsChanged, this, [this](const std::vector<sa::CurvePoint>& pts) {
        sa::Profile p = app_->profile();
        auto& ctx = p.context(editContext_->current() == 1 ? sa::ContextId::Ads : sa::ContextId::Hip);
        selectedShape(ctx, editAxis_->currentIndex()).curve.points = pts;
        app_->setProfile(p);
    });
    connect(view_, &ui::Segmented::currentChanged, this, [this](int i) {
        curve_->setView(i == 0 ? CurveEditor::View::FullResponse : CurveEditor::View::CurveShape);
    });
    connect(editContext_, &ui::Segmented::currentChanged, this, &StickLabPage::onProfile);
    connect(editAxis_, &QComboBox::currentIndexChanged, this, &StickLabPage::onProfile);
    connect(restBtn_, &QPushButton::clicked, this, [this] { runCalibration(true); });
    connect(rangeBtn_, &QPushButton::clicked, this, [this] { runCalibration(false); });
    connect(applyCenter_, &QPushButton::clicked, this, [this] {
        auto cal = app_->calibration();
        cal.right.centerX = lastRest_.mean.x;
        cal.right.centerY = lastRest_.mean.y;
        app_->setCalibration(cal);
        calStatus_->setText(calStatus_->text() + T("<br><b>Center calibration applied.</b>"));
    });
    connect(applyDeadzone_, &QPushButton::clicked, this, [this] {
        const auto& cal = app_->calibration();
        const bool centered = std::fabs(cal.right.centerX - lastRest_.mean.x) < 1e-4f && std::fabs(cal.right.centerY - lastRest_.mean.y) < 1e-4f;
        const float dz = centered ? lastRest_.recommendedInnerCalibrated : lastRest_.recommendedInnerUncalibrated;
        sa::Profile p = app_->profile();
        p.hip.deadzone.inner = dz;
        p.ads.deadzone.inner = dz;
        app_->setProfile(p);
        calStatus_->setText(calStatus_->text() + QString(T("<br><b>Inner deadzone %1 applied to hip and ADS</b>")).arg(dz, 0, 'f', 3));
    });
    connect(applyRange_, &QPushButton::clicked, this, [this] {
        auto cal = app_->calibration();
        const float cx = cal.right.centerX, cy = cal.right.centerY;
        cal.right = lastRangeCal_;
        cal.right.centerX = cx;
        cal.right.centerY = cy;
        app_->setCalibration(cal);
        sa::Profile p = app_->profile();
        if (lastRange_.recommendedOuter > p.hip.deadzone.inner + 0.06f) {
            p.hip.deadzone.outer = lastRange_.recommendedOuter;
            p.ads.deadzone.outer = lastRange_.recommendedOuter;
            app_->setProfile(p);
        }
        calStatus_->setText(calStatus_->text() + T("<br><b>Range calibration and outer threshold applied.</b>"));
    });

    connect(app_, &AppController::telemetryTick, this, &StickLabPage::onTelemetry);
    connect(app_, &AppController::profileChanged, this, &StickLabPage::onProfile);
    connect(app_, &AppController::calibrationChanged, this, &StickLabPage::onProfile);
    onProfile();
}

void StickLabPage::onProfile() {
    updating_ = true;
    std::vector<std::string> errs;
    if (auto cp = sa::CompiledProfile::compile(app_->profile(), app_->calibration(), &errs))
        curve_->setContexts(cp->context(sa::ContextId::Hip), cp->context(sa::ContextId::Ads));
    sa::Profile p = app_->profile();
    auto& ctx = p.context(editContext_->current() == 1 ? sa::ContextId::Ads : sa::ContextId::Hip);
    const bool perAxis = ctx.curveMode == sa::CurveMode::PerAxis;
    editAxis_->setEnabled(perAxis);
    if (!perAxis && editAxis_->currentIndex() != 0) editAxis_->setCurrentIndex(0);
    if (perAxis && editAxis_->currentIndex() == 0) editAxis_->setCurrentIndex(1);
    const auto& shape = selectedShape(ctx, editAxis_->currentIndex());
    shapeEdit_->setShape(shape);
    inner_->blockSignals(true);
    outer_->blockSignals(true);
    inner_->setValue(ctx.deadzone.inner);
    outer_->setValue(ctx.deadzone.outer);
    inner_->blockSignals(false);
    outer_->blockSignals(false);
    curve_->setEditableShape(shape, QString("%1 — %2").arg(editContext_->currentText(), editAxis_->currentText()));
    updating_ = false;
}

void StickLabPage::onTelemetry() {
    if (app_->capturing()) {
        calStatus_->setText(QString(T("Capturing… %1 s")).arg(app_->captureSeconds(), 0, 'f', 1));
    }
    if (!app_->hasLatest() || !isVisible()) return;
    const auto& recent = app_->recent();
    const size_t n = std::min<size_t>(recent.size(), 600);
    std::vector<sa::Vec2> raw, out, lraw, lout;
    const size_t step = std::max<size_t>(1, n / 250);
    for (size_t i = recent.size() - n; i < recent.size(); i += step) {
        raw.push_back({sa::normalizeAxis(recent[i].raw.rx), sa::normalizeAxis(recent[i].raw.ry)});
        out.push_back({sa::normalizeAxis(recent[i].out.rx), sa::normalizeAxis(recent[i].out.ry)});
        lraw.push_back({sa::normalizeAxis(recent[i].raw.lx), sa::normalizeAxis(recent[i].raw.ly)});
        lout.push_back({sa::normalizeAxis(recent[i].out.lx), sa::normalizeAxis(recent[i].out.ly)});
    }
    const auto& last = app_->latest();
    const sa::Vec2 lr{sa::normalizeAxis(last.raw.rx), sa::normalizeAxis(last.raw.ry)};
    const sa::Vec2 lo{sa::normalizeAxis(last.out.rx), sa::normalizeAxis(last.out.ry)};
    raw.push_back(lr);
    out.push_back(lo);
    const bool ads = last.context == static_cast<std::uint8_t>(sa::ContextId::Ads);
    const auto& ctx = app_->profile().context(ads ? sa::ContextId::Ads : sa::ContextId::Hip);
    lraw.push_back({sa::normalizeAxis(last.raw.lx), sa::normalizeAxis(last.raw.ly)});
    lout.push_back({sa::normalizeAxis(last.out.lx), sa::normalizeAxis(last.out.ly)});
    leftView_->setRings(app_->profile().moveStick.inner, app_->profile().moveStick.outer);
    leftView_->setPoints(lraw, lout);
    rightView_->setRings(ctx.deadzone.inner, ctx.deadzone.outer);
    rightView_->setProcessedColor(ads ? ui::Colors::ads() : ui::Colors::hip());
    rightView_->setPoints(raw, out);
    curve_->setLivePoint(sa::length(lr), sa::length(lo), ads);
    contextLabel_->setText(QString(T("<b style='color:%1'>%2</b>")).arg((ads ? ui::Colors::ads() : ui::Colors::hip()).name(), ads ? T("ADS") : T("Hip-fire")));

    // Time plot over the last ~2 s of telemetry (decimated to ~400 points).
    const size_t span = std::min<size_t>(recent.size(), 16000);
    const double seconds = span > 1 ? (recent.back().t - recent[recent.size() - span].t) * 1e-9 : 0;
    std::vector<TimePlot::Series> series(4);
    series[0] = {T("raw X"), ui::Colors::raw(), {}};
    series[1] = {T("out X"), ui::Colors::hip(), {}};
    series[2] = {T("raw Y"), ui::Colors::muted(), {}};
    series[3] = {T("out Y"), ui::Colors::ads(), {}};
    // Only keep the most recent 2 s.
    size_t start = recent.size() - span;
    while (start + 1 < recent.size() && recent.back().t - recent[start].t > 2'000'000'000) ++start;
    const size_t stride = std::max<size_t>(1, (recent.size() - start) / 400);
    for (size_t i = start; i < recent.size(); i += stride) {
        series[0].values.push_back(sa::normalizeAxis(recent[i].raw.rx));
        series[1].values.push_back(sa::normalizeAxis(recent[i].out.rx));
        series[2].values.push_back(sa::normalizeAxis(recent[i].raw.ry));
        series[3].values.push_back(sa::normalizeAxis(recent[i].out.ry));
    }
    plot_->setSeries(std::move(series), std::min(seconds, 2.0));
}

void StickLabPage::runCalibration(bool rest) {
    if (!app_->engineRunning()) {
        calStatus_->setText(T("<span style='color:#FF453A'>Start the engine (Dashboard) with your controller selected first.</span>"));
        return;
    }
    restBtn_->setEnabled(false);
    rangeBtn_->setEnabled(false);
    app_->beginCapture();
    calStatus_->setText(rest ? T("Hands off the controller…") : T("Rotate the RIGHT stick slowly around the outer edge, several full turns…"));
    QTimer::singleShot(rest ? 3000 : 6000, this, [this, rest] { finishCalibration(rest); });
}

void StickLabPage::finishCalibration(bool rest) {
    const auto samples = app_->endCapture();
    restBtn_->setEnabled(true);
    rangeBtn_->setEnabled(true);
    if (samples.size() < 20) {
        calStatus_->setText(QString(T("<span style='color:#FF453A'>Only %1 samples captured. XInput only reports changes; "
                                    "if the stick is perfectly still there may be nothing to measure.</span>"))
                                .arg(samples.size()));
        return;
    }
    const double dur = (samples.back().t - samples.front().t) * 1e-9;
    if (rest) {
        sa::RestAnalyzer ra;
        for (const auto& s : samples) ra.add({sa::normalizeAxis(s.pad.rx), sa::normalizeAxis(s.pad.ry)});
        lastRest_ = ra.report();
        const auto& r = lastRest_;
        calStatus_->setText(QString(T("<b>Rest test</b>: %1 samples in %2 s (%3/s)<br>"
                                    "resting offset (%4, %5) · noise sd (%6, %7) · max |v| %8 · max |v − mean| %9 · jitter RMS %10<br>"
                                    "Recommended inner deadzone: <b>%11</b> without center calibration, <b>%12</b> with it.%13%14"))
                                .arg(r.samples)
                                .arg(dur, 0, 'f', 1)
                                .arg(r.samples / std::max(dur, 1e-6), 0, 'f', 0)
                                .arg(r.mean.x, 0, 'f', 4)
                                .arg(r.mean.y, 0, 'f', 4)
                                .arg(r.stddev.x, 0, 'f', 4)
                                .arg(r.stddev.y, 0, 'f', 4)
                                .arg(r.maxRadiusFromZero, 0, 'f', 4)
                                .arg(r.maxRadiusFromMean, 0, 'f', 4)
                                .arg(r.diffRms, 0, 'f', 5)
                                .arg(r.recommendedInnerUncalibrated, 0, 'f', 3)
                                .arg(r.recommendedInnerCalibrated, 0, 'f', 3)
                                .arg(r.driftSuspected ? T("<br><span style='color:#FFD60A'>Resting offset is large: stick drift suspected.</span>") : QString())
                                .arg(r.noisy ? T("<br><span style='color:#FFD60A'>High sample-to-sample jitter.</span>") : QString()));
        applyCenter_->setEnabled(true);
        applyDeadzone_->setEnabled(true);
    } else {
        sa::RangeAnalyzer ra;
        for (const auto& s : samples) ra.add({sa::normalizeAxis(s.pad.rx), sa::normalizeAxis(s.pad.ry)});
        lastRange_ = ra.report();
        lastRangeCal_ = ra.toCalibration({});
        const auto& r = lastRange_;
        calStatus_->setText(QString(T("<b>Range sweep</b>: %1 samples · x [%2, %3] · y [%4, %5]<br>"
                                    "directions covered %6/%7 · weakest direction reaches %8 · circularity error %9%<br>"
                                    "Recommended outer threshold: <b>%10</b>%11"))
                                .arg(r.samples)
                                .arg(r.minX, 0, 'f', 3)
                                .arg(r.maxX, 0, 'f', 3)
                                .arg(r.minY, 0, 'f', 3)
                                .arg(r.maxY, 0, 'f', 3)
                                .arg(r.sectorsCovered)
                                .arg(sa::RangeReport::kSectors)
                                .arg(r.minSectorMax, 0, 'f', 3)
                                .arg(r.circularityError * 100, 0, 'f', 1)
                                .arg(r.recommendedOuter, 0, 'f', 3)
                                .arg(r.sectorsCovered < sa::RangeReport::kSectors
                                         ? T("<br><span style='color:#FFD60A'>Not all directions reached: repeat more slowly.</span>")
                                         : QString()));
        applyRange_->setEnabled(r.sectorsCovered == sa::RangeReport::kSectors);
    }
}
