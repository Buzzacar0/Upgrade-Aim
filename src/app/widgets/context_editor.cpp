#include "widgets/context_editor.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "widgets/grouped_list.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

template <typename W>
void block(std::initializer_list<W*> ws, bool on) {
    for (auto* w : ws) w->blockSignals(on);
}

}  // namespace

// ------------------------------------------------------------------ ShapeEditor

ShapeEditor::ShapeEditor(const QString& header, QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto* g = new ui::Group(header);
    v->addWidget(g);
    type_ = new QComboBox;
    type_->addItems({T("Linear"), T("Power"), T("Exponential"), T("Custom (edit in graph)")});
    g->addRow(T("Curve"), type_);
    exponent_ = ui::spin(0.1, 10.0, 0.05, 2, 1.0);
    expRow_ = g->addRow(T("Exponent"), exponent_, T("Above 1 = slower start, more precision near the center."));
    k_ = ui::spin(-12.0, 12.0, 0.1, 2, 0.0);
    kRow_ = g->addRow(T("Exponential k"), k_, T("Above 0 = slower start, below 0 = faster start."));

    fineOn_ = new QCheckBox;
    g->addRow(T("Fine-adjustment band"), fineOn_, T("Near the center: more stick travel per step, for finer corrections."));
    fineEnd_ = ui::spin(0.02, 0.6, 0.01, 2, 0.25);
    fineGain_ = ui::spin(0.05, 4.0, 0.05, 2, 0.6);
    fineBlend_ = ui::spin(0.0, 0.3, 0.01, 2, 0.08);
    fineRows_ << g->addRow(T("Band ends at"), fineEnd_) << g->addRow(T("Slope gain"), fineGain_)
              << g->addRow(T("Blend"), fineBlend_);

    fastOn_ = new QCheckBox;
    g->addRow(T("Fast-movement band"), fastOn_, T("Near the rim: reach full output with less travel."));
    fastStart_ = ui::spin(0.4, 0.98, 0.01, 2, 0.8);
    fastGain_ = ui::spin(0.05, 20.0, 0.05, 2, 1.6);
    fastBlend_ = ui::spin(0.0, 0.3, 0.01, 2, 0.08);
    fastRows_ << g->addRow(T("Band starts at"), fastStart_) << g->addRow(T("Slope gain"), fastGain_)
              << g->addRow(T("Blend"), fastBlend_);

    auto emitChanged = [this] {
        syncEnabled();
        Q_EMIT changed();
    };
    connect(type_, &QComboBox::currentIndexChanged, this, emitChanged);
    for (auto* s : {exponent_, k_, fineEnd_, fineGain_, fineBlend_, fastStart_, fastGain_, fastBlend_})
        connect(s, &QDoubleSpinBox::valueChanged, this, emitChanged);
    connect(fineOn_, &QCheckBox::toggled, this, emitChanged);
    connect(fastOn_, &QCheckBox::toggled, this, emitChanged);
    syncEnabled();
}

void ShapeEditor::syncEnabled() {
    // Rows that do not apply are hidden rather than greyed out.
    const int t = type_->currentIndex();
    expRow_->setVisible(t == 1);
    kRow_->setVisible(t == 2);
    for (auto* r : fineRows_) r->setVisible(fineOn_->isChecked());
    for (auto* r : fastRows_) r->setVisible(fastOn_->isChecked());
}
void ShapeEditor::setShape(const sa::ResponseShape& s) {
    block<QWidget>({type_, exponent_, k_, fineOn_, fineEnd_, fineGain_, fineBlend_, fastOn_, fastStart_, fastGain_, fastBlend_}, true);
    type_->setCurrentIndex(static_cast<int>(s.curve.type));
    exponent_->setValue(s.curve.exponent);
    k_->setValue(s.curve.expK);
    points_ = s.curve.points;
    fineOn_->setChecked(s.fine.enabled);
    fineEnd_->setValue(s.fine.end);
    fineGain_->setValue(s.fine.gain);
    fineBlend_->setValue(s.fine.blend);
    fastOn_->setChecked(s.fast.enabled);
    fastStart_->setValue(s.fast.start);
    fastGain_->setValue(s.fast.gain);
    fastBlend_->setValue(s.fast.blend);
    block<QWidget>({type_, exponent_, k_, fineOn_, fineEnd_, fineGain_, fineBlend_, fastOn_, fastStart_, fastGain_, fastBlend_}, false);
    syncEnabled();
}

sa::ResponseShape ShapeEditor::shape() const {
    sa::ResponseShape s;
    s.curve.type = static_cast<sa::CurveType>(type_->currentIndex());
    s.curve.exponent = static_cast<float>(exponent_->value());
    s.curve.expK = static_cast<float>(k_->value());
    s.curve.points = points_;
    if (s.curve.type == sa::CurveType::Custom && s.curve.points.empty())
        s.curve.points = {{0.25f, 0.12f}, {0.5f, 0.38f}, {0.75f, 0.7f}};  // sensible starting S-ish curve
    s.fine = {fineOn_->isChecked(), 0.0f, static_cast<float>(fineEnd_->value()), static_cast<float>(fineGain_->value()),
              static_cast<float>(fineBlend_->value())};
    s.fast = {fastOn_->isChecked(), static_cast<float>(fastStart_->value()), 1.0f, static_cast<float>(fastGain_->value()),
              static_cast<float>(fastBlend_->value())};
    return s;
}

// ---------------------------------------------------------------- ContextEditor

ContextEditor::ContextEditor(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(22);

    auto* dz = new ui::Group(T("Deadzone"));
    dzType_ = new QComboBox;
    dzType_->addItems({T("None (saturation only)"), T("Radial"), T("Axial")});
    dz->addRow(T("Type"), dzType_);
    dzInner_ = ui::spin(0.0, 0.85, 0.005, 3, 0.05);
    innerRow_ = dz->addRow(T("Inner deadzone"), dzInner_, T("Ignore stick noise below this deflection."));
    dzOuter_ = ui::spin(0.15, 1.0, 0.005, 3, 0.98);
    dz->addRow(T("Outer threshold"), dzOuter_, T("Deflection that already counts as full."));
    anti_ = ui::spin(0.0, 0.4, 0.005, 3, 0.0);
    dz->addRow(T("Anti-deadzone"), anti_, T("Only to cancel a game deadzone that cannot be set to 0."));
    dz->setFooter(T("Radial preserves direction; axial suppresses each axis independently (can snap diagonals to axes)."));
    root->addWidget(dz);

    radial_ = new ShapeEditor(T("Response curve"));
    x_ = new ShapeEditor(T("Horizontal (X) curve"));
    y_ = new ShapeEditor(T("Vertical (Y) curve"));
    root->addWidget(radial_);
    root->addWidget(x_);
    root->addWidget(y_);

    auto* sens = new ui::Group(T("Sensitivity"));
    sensX_ = ui::spin(0.1, 4.0, 0.01, 2, 1.0);
    sens->addRow(T("Horizontal"), sensX_);
    sensY_ = ui::spin(0.1, 4.0, 0.01, 2, 1.0);
    sens->addRow(T("Vertical"), sensY_);
    curveMode_ = new QComboBox;
    curveMode_->addItems({T("Radial (one curve on magnitude)"), T("Per axis (separate X / Y curves)")});
    sens->addRow(T("Curve mode"), curveMode_);
    circular_ = new QCheckBox;
    sens->addRow(T("Circular saturation"), circular_, T("Keeps diagonals from exceeding full deflection."));
    sens->setFooter(T("Output never exceeds full deflection; above 1 reaches it sooner."));
    root->addWidget(sens);

    auto* sm = new ui::Group(T("Smoothing"));
    smoothType_ = new QComboBox;
    smoothType_->addItems({T("Off (zero added latency)"), T("EMA low-pass"), T("One Euro (adaptive)")});
    sm->addRow(T("Filter"), smoothType_);
    emaTau_ = ui::spin(0.1, 100.0, 0.5, 1, 4.0);
    emaRow_ = sm->addRow(T("EMA time constant (ms)"), emaTau_);
    minCutoff_ = ui::spin(0.05, 60.0, 0.1, 2, 3.0);
    beta_ = ui::spin(0.0, 5.0, 0.01, 3, 0.05);
    dCutoff_ = ui::spin(0.1, 60.0, 0.5, 1, 10.0);
    euroRows_ << sm->addRow(T("One Euro min cutoff (Hz)"), minCutoff_) << sm->addRow(T("One Euro beta"), beta_)
              << sm->addRow(T("One Euro derivative cutoff (Hz)"), dCutoff_);
    flick_ = ui::spin(0.0, 200.0, 1.0, 1, 0.0);
    flickRow_ = sm->addRow(T("Flick bypass speed (/s)"), flick_, T("Above this stick speed smoothing is skipped. 0 = off."));
    sm->setFooter(T("Smoothing is the only setting that adds delay. The Testing page measures how much."));
    root->addWidget(sm);
    root->addStretch();

    auto emitChanged = [this] {
        syncEnabled();
        Q_EMIT changed();
    };
    for (auto* c : {dzType_, curveMode_, smoothType_}) connect(c, &QComboBox::currentIndexChanged, this, emitChanged);
    for (auto* s : {dzInner_, dzOuter_, sensX_, sensY_, anti_, emaTau_, minCutoff_, beta_, dCutoff_, flick_})
        connect(s, &QDoubleSpinBox::valueChanged, this, emitChanged);
    connect(circular_, &QCheckBox::toggled, this, emitChanged);
    for (auto* e : {radial_, x_, y_}) connect(e, &ShapeEditor::changed, this, emitChanged);
    syncEnabled();
}

void ContextEditor::syncEnabled() {
    const bool perAxis = curveMode_->currentIndex() == 1;
    radial_->setVisible(!perAxis);
    x_->setVisible(perAxis);
    y_->setVisible(perAxis);
    const int sm = smoothType_->currentIndex();
    emaRow_->setVisible(sm == 1);
    for (auto* r : euroRows_) r->setVisible(sm == 2);
    flickRow_->setVisible(sm != 0);
    innerRow_->setVisible(dzType_->currentIndex() != 0);
}
void ContextEditor::setConfig(const sa::AimContextConfig& c) {
    base_ = c;
    const std::initializer_list<QWidget*> ws = {dzType_, curveMode_, smoothType_, dzInner_, dzOuter_, sensX_, sensY_, anti_,
                                                emaTau_, minCutoff_, beta_, dCutoff_, flick_, circular_, radial_, x_, y_};
    block<QWidget>(ws, true);
    dzType_->setCurrentIndex(static_cast<int>(c.deadzone.type));
    dzInner_->setValue(c.deadzone.inner);
    dzOuter_->setValue(c.deadzone.outer);
    anti_->setValue(c.antiDeadzone);
    sensX_->setValue(c.sensX);
    sensY_->setValue(c.sensY);
    circular_->setChecked(c.circularSaturation);
    curveMode_->setCurrentIndex(c.curveMode == sa::CurveMode::PerAxis ? 1 : 0);
    smoothType_->setCurrentIndex(static_cast<int>(c.smoothing.type));
    emaTau_->setValue(c.smoothing.emaTauMs);
    minCutoff_->setValue(c.smoothing.minCutoffHz);
    beta_->setValue(c.smoothing.beta);
    dCutoff_->setValue(c.smoothing.dCutoffHz);
    flick_->setValue(c.smoothing.flickBypassSpeed);
    radial_->setShape(c.radial);
    x_->setShape(c.x);
    y_->setShape(c.y);
    block<QWidget>(ws, false);
    syncEnabled();
}

sa::AimContextConfig ContextEditor::config() const {
    sa::AimContextConfig c = base_;
    c.deadzone.type = static_cast<sa::DeadzoneType>(dzType_->currentIndex());
    c.deadzone.inner = static_cast<float>(dzInner_->value());
    c.deadzone.outer = static_cast<float>(dzOuter_->value());
    c.antiDeadzone = static_cast<float>(anti_->value());
    c.sensX = static_cast<float>(sensX_->value());
    c.sensY = static_cast<float>(sensY_->value());
    c.circularSaturation = circular_->isChecked();
    c.curveMode = curveMode_->currentIndex() == 1 ? sa::CurveMode::PerAxis : sa::CurveMode::Radial;
    c.smoothing.type = static_cast<sa::SmoothingType>(smoothType_->currentIndex());
    c.smoothing.emaTauMs = static_cast<float>(emaTau_->value());
    c.smoothing.minCutoffHz = static_cast<float>(minCutoff_->value());
    c.smoothing.beta = static_cast<float>(beta_->value());
    c.smoothing.dCutoffHz = static_cast<float>(dCutoff_->value());
    c.smoothing.flickBypassSpeed = static_cast<float>(flick_->value());
    c.radial = radial_->shape();
    c.x = x_->shape();
    c.y = y_->shape();
    return c;
}
