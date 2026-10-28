#pragma once

#include <QWidget>

#include "stickyaim/core/profile.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;

// Editor for one response shape: curve type + parameters + fine/fast zones.
class ShapeEditor : public QWidget {
    Q_OBJECT
public:
    explicit ShapeEditor(QWidget* parent = nullptr);
    void setShape(const sa::ResponseShape& s);
    sa::ResponseShape shape() const;

Q_SIGNALS:
    void changed();

private:
    void syncEnabled();
    QComboBox* type_;
    QDoubleSpinBox *exponent_, *k_;
    QCheckBox* fineOn_;
    QDoubleSpinBox *fineEnd_, *fineGain_, *fineBlend_;
    QCheckBox* fastOn_;
    QDoubleSpinBox *fastStart_, *fastGain_, *fastBlend_;
    std::vector<sa::CurvePoint> points_;  // preserved; edited graphically
};

// Editor for a complete aiming context (deadzone, curves, sensitivity,
// smoothing, saturation).
class ContextEditor : public QWidget {
    Q_OBJECT
public:
    explicit ContextEditor(QWidget* parent = nullptr);
    void setConfig(const sa::AimContextConfig& c);
    sa::AimContextConfig config() const;

Q_SIGNALS:
    void changed();

private:
    void syncEnabled();
    sa::AimContextConfig base_;
    QComboBox *dzType_, *curveMode_, *smoothType_;
    QDoubleSpinBox *dzInner_, *dzOuter_, *sensX_, *sensY_, *anti_;
    QDoubleSpinBox *emaTau_, *minCutoff_, *beta_, *dCutoff_, *flick_;
    QCheckBox* circular_;
    ShapeEditor *radial_, *x_, *y_;
    QGroupBox *radialBox_, *xBox_, *yBox_;
};
