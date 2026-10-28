#include "widgets/context_editor.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

template <typename W>
void block(std::initializer_list<W*> ws, bool on) {
    for (auto* w : ws) w->blockSignals(on);
}

}  // namespace

// ------------------------------------------------------------------ ShapeEditor

ShapeEditor::ShapeEditor(QWidget* parent) : QWidget(parent) {
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    type_ = new QComboBox;
    type_->addItems({T("Linear"), T("Power"), T("Exponential"), T("Custom (edit in graph)")});
    exponent_ = ui::spin(0.1, 10.0, 0.05, 2, 1.0);
    exponent_->setToolTip(T("f(t) = t^exponent. >1 = slower start (more precision near center), <1 = faster start."));
    k_ = ui::spin(-12.0, 12.0, 0.1, 2, 0.0);
    k_->setToolTip(T("f(t) = (e^(k t) - 1) / (e^k - 1). k>0 = slower start, k<0 = faster start, 0 = linear."));
    form->addRow(T("Curve"), type_);
    form->addRow(T("Exponent"), exponent_);
    form->addRow(T("Exponential k"), k_);

    fineOn_ = new QCheckBox(T("Fine-adjustment band (near center)"));
    fineEnd_ = ui::spin(0.02, 0.6, 0.01, 2, 0.25);
    fineGain_ = ui::spin(0.05, 4.0, 0.05, 2, 0.6);
    fineGain_->setToolTip(T("Slope multiplier inside the band. <1 = more stick travel per output step (finer corrections)."));
    fineBlend_ = ui::spin(0.0, 0.3, 0.01, 2, 0.08);
    form->addRow(fineOn_);
    form->addRow(T("  band end"), fineEnd_);
    form->addRow(T("  slope gain"), fineGain_);
    form->addRow("  blend", fineBlend_);

    fastOn_ = new QCheckBox(T("Fast-movement band (near rim)"));
    fastStart_ = ui::spin(0.4, 0.98, 0.01, 2, 0.8);
    fastGain_ = ui::spin(0.05, 20.0, 0.05, 2, 1.6);
    fastGain_->setToolTip(T("Slope multiplier near the rim. >1 = reach full output with less travel."));
    fastBlend_ = ui::spin(0.0, 0.3, 0.01, 2, 0.08);
    form->addRow(fastOn_);
    form->addRow(T("  band start"), fastStart_);
    form->addRow(T("  slope gain"), fastGain_);
    form->addRow("  blend", fastBlend_);

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
    const int t = type_->currentIndex();
    exponent_->setEnabled(t == 1);
    k_->setEnabled(t == 2);
    for (auto* w : {fineEnd_, fineGain_, fineBlend_}) w->setEnabled(fineOn_->isChecked());
    for (auto* w : {fastStart_, fastGain_, fastBlend_}) w->setEnabled(fastOn_->isChecked());
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
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    root->addLayout(left, 1);
    root->addLayout(right, 1);

    auto* dzBox = new QGroupBox(T("Deadzone and thresholds"));
    auto* dzForm = new QFormLayout(dzBox);
    dzType_ = new QComboBox;
    dzType_->addItems({T("None (saturation only)"), T("Radial"), T("Axial")});
    dzType_->setToolTip(T("Radial preserves direction; axial suppresses each axis independently (can snap diagonals to axes)."));
    dzInner_ = ui::spin(0.0, 0.85, 0.005, 3, 0.05);
    dzOuter_ = ui::spin(0.15, 1.0, 0.005, 3, 0.98);
    dzForm->addRow(T("Type"), dzType_);
    dzForm->addRow(T("Inner deadzone"), dzInner_);
    dzForm->addRow(T("Outer threshold"), dzOuter_);
    anti_ = ui::spin(0.0, 0.4, 0.005, 3, 0.0);
    anti_->setToolTip(T("Minimum output once outside the deadzone. Only use to cancel a game deadzone that cannot be set to 0."));
    dzForm->addRow(T("Anti-deadzone"), anti_);
    left->addWidget(dzBox);

    auto* sensBox = new QGroupBox(T("Sensitivity and saturation"));
    auto* sensForm = new QFormLayout(sensBox);
    sensX_ = ui::spin(0.1, 4.0, 0.01, 2, 1.0);
    sensY_ = ui::spin(0.1, 4.0, 0.01, 2, 1.0);
    sensX_->setToolTip(T("Output gain on the horizontal axis. Output never exceeds full deflection; >1 reaches it sooner."));
    circular_ = new QCheckBox(T("Circular saturation (clamp |output| to 1)"));
    curveMode_ = new QComboBox;
    curveMode_->addItems({T("Radial (one curve on magnitude)"), T("Per axis (separate X / Y curves)")});
    sensForm->addRow(T("Horizontal"), sensX_);
    sensForm->addRow(T("Vertical"), sensY_);
    sensForm->addRow(circular_);
    sensForm->addRow(T("Curve mode"), curveMode_);
    left->addWidget(sensBox);

    auto* smBox = new QGroupBox(T("Smoothing (adds latency — measured in Testing)"));
    auto* smForm = new QFormLayout(smBox);
    smoothType_ = new QComboBox;
    smoothType_->addItems({T("Off (zero added latency)"), T("EMA low-pass"), T("One Euro (adaptive)")});
    emaTau_ = ui::spin(0.1, 100.0, 0.5, 1, 4.0);
    minCutoff_ = ui::spin(0.05, 60.0, 0.1, 2, 3.0);
    beta_ = ui::spin(0.0, 5.0, 0.01, 3, 0.05);
    dCutoff_ = ui::spin(0.1, 60.0, 0.5, 1, 10.0);
    flick_ = ui::spin(0.0, 200.0, 1.0, 1, 0.0);
    flick_->setToolTip(T("Stick speed (full deflections per second) above which smoothing is bypassed. 0 = off."));
    smForm->addRow(T("Filter"), smoothType_);
    smForm->addRow(T("EMA time constant (ms)"), emaTau_);
    smForm->addRow(T("One Euro min cutoff (Hz)"), minCutoff_);
    smForm->addRow(T("One Euro beta"), beta_);
    smForm->addRow(T("One Euro derivative cutoff (Hz)"), dCutoff_);
    smForm->addRow(T("Flick bypass speed (/s)"), flick_);
    left->addWidget(smBox);
    left->addStretch();

    radialBox_ = new QGroupBox(T("Response curve (magnitude)"));
    radial_ = new ShapeEditor;
    (new QVBoxLayout(radialBox_))->addWidget(radial_);
    xBox_ = new QGroupBox(T("Horizontal (X) curve"));
    x_ = new ShapeEditor;
    (new QVBoxLayout(xBox_))->addWidget(x_);
    yBox_ = new QGroupBox(T("Vertical (Y) curve"));
    y_ = new ShapeEditor;
    (new QVBoxLayout(yBox_))->addWidget(y_);
    right->addWidget(radialBox_);
    right->addWidget(xBox_);
    right->addWidget(yBox_);
    right->addStretch();

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
    radialBox_->setVisible(!perAxis);
    xBox_->setVisible(perAxis);
    yBox_->setVisible(perAxis);
    const int sm = smoothType_->currentIndex();
    emaTau_->setEnabled(sm == 1);
    for (auto* w : {minCutoff_, beta_, dCutoff_}) w->setEnabled(sm == 2);
    flick_->setEnabled(sm != 0);
    dzInner_->setEnabled(dzType_->currentIndex() != 0);
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
