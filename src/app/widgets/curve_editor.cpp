#include "widgets/curve_editor.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {
constexpr int kSamples = 400;
constexpr double kHitRadius = 9.0;
}  // namespace

CurveEditor::CurveEditor(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(320, 260);
}

void CurveEditor::setContexts(const sa::CompiledContext& hip, const sa::CompiledContext& ads) {
    hip_ = hip;
    ads_ = ads;
    update();
}

void CurveEditor::setEditableShape(const sa::ResponseShape& shape, const QString& label) {
    shape_ = shape;
    shape_.curve = sa::sanitize(shape.curve);
    shapeLut_ = sa::ResponseLut::build(shape_);
    shapeLabel_ = label;
    if (dragIndex_ < 0) points_ = shape_.curve.points;
    update();
}

void CurveEditor::setLivePoint(float in, float out, bool ads) {
    live_ = true;
    liveIn_ = in;
    liveOut_ = out;
    liveAds_ = ads;
    update();
}

void CurveEditor::clearLivePoint() {
    live_ = false;
    update();
}

void CurveEditor::setView(View v) {
    view_ = v;
    update();
}

QRectF CurveEditor::plotRect() const {
    const double left = 46, top = 28, right = 16, bottom = 36;
    const double w = width() - left - right, h = height() - top - bottom;
    const double side = std::min(w, h);
    return {left + (w - side) / 2, top, side, side};
}

QPointF CurveEditor::toScreen(double x, double y) const {
    const QRectF r = plotRect();
    return {r.left() + x * r.width(), r.bottom() - y * r.height()};
}

std::pair<double, double> CurveEditor::toData(QPointF p) const {
    const QRectF r = plotRect();
    return {(p.x() - r.left()) / r.width(), (r.bottom() - p.y()) / r.height()};
}

bool CurveEditor::pointsEditable() const { return view_ == View::CurveShape && shape_.curve.type == sa::CurveType::Custom; }

int CurveEditor::hitPoint(QPointF pos) const {
    for (size_t i = 0; i < points_.size(); ++i) {
        const QPointF s = toScreen(points_[i].x, points_[i].y);
        if (std::hypot(s.x() - pos.x(), s.y() - pos.y()) <= kHitRadius) return static_cast<int>(i);
    }
    return -1;
}

void CurveEditor::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    ui::paintCard(p, rect());
    const QRectF r = plotRect();

    // Grid and axes.
    for (int i = 0; i <= 10; ++i) {
        const double v = i / 10.0;
        p.setPen(QPen(i % 5 == 0 ? ui::Colors::axis() : ui::Colors::grid(), 1));
        p.drawLine(toScreen(v, 0), toScreen(v, 1));
        p.drawLine(toScreen(0, v), toScreen(1, v));
        if (i % 2 == 0) {
            p.setPen(ui::Colors::muted());
            p.drawText(QRectF(toScreen(v, 0).x() - 15, r.bottom() + 4, 30, 14), Qt::AlignCenter, QString::number(v, 'f', 1));
            p.drawText(QRectF(r.left() - 40, toScreen(0, v).y() - 7, 34, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', 1));
        }
    }
    p.setPen(QPen(ui::Colors::muted(), 1, Qt::DashLine));
    p.drawLine(toScreen(0, 0), toScreen(1, 1));
    p.setPen(ui::Colors::muted());
    p.drawText(QRectF(r.left(), r.bottom() + 18, r.width(), 16), Qt::AlignCenter,
               view_ == View::FullResponse ? T("raw stick deflection (magnitude)") : T("curve input (after deadzone)"));
    p.save();
    p.translate(12, r.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-r.height() / 2, -8, r.height(), 16), Qt::AlignCenter, T("output"));
    p.restore();

    auto drawFn = [&](auto fn, QColor col, Qt::PenStyle style, double width) {
        QPainterPath path;
        for (int i = 0; i <= kSamples; ++i) {
            const double x = static_cast<double>(i) / kSamples;
            const QPointF s = toScreen(x, std::clamp(static_cast<double>(fn(static_cast<float>(x))), 0.0, 1.0));
            if (i == 0) path.moveTo(s);
            else path.lineTo(s);
        }
        p.setPen(QPen(col, width, style));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    };

    if (view_ == View::FullResponse && hip_ && ads_) {
        auto along = [](const sa::CompiledContext& c, bool yAxis) {
            return [&c, yAxis](float m) {
                const sa::Vec2 v = sa::shapeAimStick(yAxis ? sa::Vec2{0, m} : sa::Vec2{m, 0}, c);
                return yAxis ? std::fabs(v.y) : std::fabs(v.x);
            };
        };
        drawFn(along(*hip_, false), ui::Colors::hip(), Qt::SolidLine, 2.2);
        drawFn(along(*ads_, false), ui::Colors::ads(), Qt::SolidLine, 2.2);
        if (hip_->cfg.curveMode == sa::CurveMode::PerAxis || hip_->cfg.sensX != hip_->cfg.sensY)
            drawFn(along(*hip_, true), ui::Colors::hip(), Qt::DashLine, 1.4);
        if (ads_->cfg.curveMode == sa::CurveMode::PerAxis || ads_->cfg.sensX != ads_->cfg.sensY)
            drawFn(along(*ads_, true), ui::Colors::ads(), Qt::DashLine, 1.4);

        // Deadzone markers for the hip context.
        p.setPen(QPen(ui::Colors::warn(), 1, Qt::DotLine));
        p.drawLine(toScreen(hip_->cfg.deadzone.inner, 0), toScreen(hip_->cfg.deadzone.inner, 1));
        p.setPen(QPen(ui::Colors::good(), 1, Qt::DotLine));
        p.drawLine(toScreen(hip_->cfg.deadzone.outer, 0), toScreen(hip_->cfg.deadzone.outer, 1));

        // Legend above the plot.
        const QString hipText = T("Hip-fire (solid = X, dashed = Y)");
        p.setPen(ui::Colors::hip());
        p.drawText(QPointF(r.left(), r.top() - 10), hipText);
        p.setPen(ui::Colors::ads());
        p.drawText(QPointF(r.left() + p.fontMetrics().horizontalAdvance(hipText) + 18, r.top() - 10), T("ADS"));

        if (live_) {
            const QColor col = liveAds_ ? ui::Colors::ads() : ui::Colors::hip();
            const QPointF s = toScreen(std::clamp(liveIn_, 0.0f, 1.0f), std::clamp(liveOut_, 0.0f, 1.0f));
            p.setPen(QPen(col.lighter(130), 1, Qt::DotLine));
            p.drawLine(QPointF(s.x(), r.bottom()), s);
            p.drawLine(QPointF(r.left(), s.y()), s);
            p.setPen(QPen(Qt::white, 1.5));
            p.setBrush(col);
            p.drawEllipse(s, 6, 6);
            p.setPen(ui::Colors::text());
            p.drawText(QRectF(r.right() - 300, r.bottom() - 20, 296, 16), Qt::AlignRight,
                       QString(T("live: in %1 → out %2")).arg(liveIn_, 0, 'f', 3).arg(liveOut_, 0, 'f', 3));
        }
    } else if (view_ == View::CurveShape) {
        const sa::Curve base = sa::Curve::fromSpec(shape_.curve);
        if (shape_.fine.enabled || shape_.fast.enabled)
            drawFn([&](float t) { return base(t); }, ui::Colors::muted(), Qt::DashLine, 1.2);
        drawFn([&](float t) { return shapeLut_.eval(t); }, ui::Colors::hip(), Qt::SolidLine, 2.4);
        auto band = [&](const sa::SlopeZone& z, QColor col) {
            if (!z.enabled) return;
            QColor f = col;
            f.setAlphaF(0.10f);
            p.fillRect(QRectF(toScreen(z.start, 1), toScreen(z.end, 0)), f);
        };
        band(shape_.fine, ui::Colors::good());
        band(shape_.fast, ui::Colors::bad());
        p.setPen(ui::Colors::text());
        p.drawText(QPointF(r.left() + 8, r.top() + 16), shapeLabel_);
        if (shape_.fine.enabled || shape_.fast.enabled) {
            p.setPen(ui::Colors::muted());
            p.drawText(QPointF(r.left() + 8, r.top() + 32), T("dashed = base curve, solid = with zones (green fine, red fast)"));
        }
        if (pointsEditable()) {
            for (size_t i = 0; i < points_.size(); ++i) {
                const QPointF s = toScreen(points_[i].x, points_[i].y);
                p.setPen(QPen(Qt::white, 1.5));
                p.setBrush(static_cast<int>(i) == dragIndex_ ? ui::Colors::ads() : ui::Colors::hip());
                p.drawEllipse(s, 6, 6);
            }
            p.setPen(ui::Colors::muted());
            p.drawText(QRectF(r.left(), r.top() - 22, r.width(), 16), Qt::AlignCenter,
                       T("drag points • double-click to add • right-click to remove"));
        } else {
            p.setPen(ui::Colors::muted());
            p.drawText(QRectF(r.left(), r.top() - 22, r.width(), 16), Qt::AlignCenter,
                       T("set the curve type to Custom to edit points"));
        }
    }

    if (hover_ && r.contains(*hover_)) {
        const auto [x, y] = toData(*hover_);
        p.setPen(ui::Colors::muted());
        p.drawText(QRectF(r.left() + 8, r.bottom() - 20, 160, 16), Qt::AlignLeft,
                   QString(T("cursor %1, %2")).arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    }
}

void CurveEditor::emitPoints() {
    std::vector<sa::CurvePoint> sorted = points_;
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.x < b.x; });
    Q_EMIT pointsChanged(sorted);
}

void CurveEditor::mousePressEvent(QMouseEvent* e) {
    if (!pointsEditable()) return;
    const int hit = hitPoint(e->position());
    if (e->button() == Qt::RightButton && hit >= 0) {
        points_.erase(points_.begin() + hit);
        emitPoints();
        update();
        return;
    }
    if (e->button() == Qt::LeftButton) dragIndex_ = hit;
}

void CurveEditor::mouseMoveEvent(QMouseEvent* e) {
    hover_ = e->position();
    if (dragIndex_ >= 0 && pointsEditable()) {
        auto [x, y] = toData(e->position());
        // Keep x strictly between neighbours and y monotone.
        const float lo = dragIndex_ > 0 ? points_[dragIndex_ - 1].x + 0.01f : 0.01f;
        const float hi = dragIndex_ + 1 < static_cast<int>(points_.size()) ? points_[dragIndex_ + 1].x - 0.01f : 0.99f;
        const float ylo = dragIndex_ > 0 ? points_[dragIndex_ - 1].y : 0.0f;
        const float yhi = dragIndex_ + 1 < static_cast<int>(points_.size()) ? points_[dragIndex_ + 1].y : 1.0f;
        points_[dragIndex_].x = std::clamp(static_cast<float>(x), lo, std::max(lo, hi));
        points_[dragIndex_].y = std::clamp(static_cast<float>(y), ylo, yhi);
        emitPoints();
    }
    update();
}

void CurveEditor::mouseReleaseEvent(QMouseEvent*) { dragIndex_ = -1; }

void CurveEditor::mouseDoubleClickEvent(QMouseEvent* e) {
    if (!pointsEditable() || points_.size() >= 16) return;
    auto [x, y] = toData(e->position());
    if (x <= 0.01 || x >= 0.99) return;
    points_.push_back({static_cast<float>(x), static_cast<float>(std::clamp(y, 0.0, 1.0))});
    std::sort(points_.begin(), points_.end(), [](auto& a, auto& b) { return a.x < b.x; });
    // Re-impose monotonicity around the new point.
    float run = 0;
    for (auto& pt : points_) {
        run = std::max(run, pt.y);
        pt.y = run;
    }
    emitPoints();
    update();
}

void CurveEditor::leaveEvent(QEvent*) {
    hover_.reset();
    update();
}
