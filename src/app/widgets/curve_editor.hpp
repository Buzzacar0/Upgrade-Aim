#pragma once

#include <QWidget>

#include <optional>
#include <vector>

#include "stickyaim/core/pipeline.hpp"

// Interactive response graph (input magnitude -> output magnitude).
//
//  * "Full response" view: the complete static pipeline (deadzone, curve,
//    zones, sensitivity, saturation, anti-deadzone) for hip and ADS, plus the
//    live raw->processed point from the controller.
//  * "Curve shape" view: only the curve + zones of the shape being edited. When
//    that curve is Custom its control points can be dragged; double-click adds
//    a point, right-click removes one.
class CurveEditor : public QWidget {
    Q_OBJECT
public:
    enum class View { FullResponse, CurveShape };

    explicit CurveEditor(QWidget* parent = nullptr);

    void setContexts(const sa::CompiledContext& hip, const sa::CompiledContext& ads);
    void setEditableShape(const sa::ResponseShape& shape, const QString& label);
    void setLivePoint(float in, float out, bool ads);
    void clearLivePoint();
    void setView(View v);
    View view() const { return view_; }

    QSize sizeHint() const override { return {520, 420}; }

Q_SIGNALS:
    void pointsChanged(const std::vector<sa::CurvePoint>& points);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QRectF plotRect() const;
    QPointF toScreen(double x, double y) const;
    std::pair<double, double> toData(QPointF p) const;
    int hitPoint(QPointF pos) const;
    bool pointsEditable() const;
    void emitPoints();

    View view_ = View::FullResponse;
    std::optional<sa::CompiledContext> hip_, ads_;
    sa::ResponseShape shape_;
    sa::ResponseLut shapeLut_;
    QString shapeLabel_;
    std::vector<sa::CurvePoint> points_;
    int dragIndex_ = -1;
    bool live_ = false;
    float liveIn_ = 0, liveOut_ = 0;
    bool liveAds_ = false;
    std::optional<QPointF> hover_;
};
