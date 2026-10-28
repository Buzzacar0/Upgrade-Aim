#include "widgets/time_plot.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

#include "i18n.hpp"
#include "widgets/ui_helpers.hpp"

TimePlot::TimePlot(QString title, double yMin, double yMax, QWidget* parent)
    : QWidget(parent), title_(std::move(title)), yMin_(yMin), yMax_(yMax) {
    setMinimumHeight(140);
}

void TimePlot::setSeries(std::vector<Series> s, double spanSeconds) {
    series_ = std::move(s);
    span_ = spanSeconds;
    update();
}

void TimePlot::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    ui::paintCard(p, rect());
    // Title on the first line, legend on the second, plot below.
    const QRectF r(48, 58, width() - 66, height() - 84);
    auto y = [&](double v) { return r.bottom() - (std::clamp(v, yMin_, yMax_) - yMin_) / (yMax_ - yMin_) * r.height(); };

    QFont titleFont = font();
    titleFont.setPixelSize(12);
    titleFont.setWeight(QFont::DemiBold);
    QFont smallFont = font();
    smallFont.setPixelSize(11);
    p.setFont(titleFont);
    p.setPen(ui::Colors::muted());
    p.drawText(QRectF(18, 12, width() - 36, 18), Qt::AlignLeft | Qt::AlignVCenter, title_);
    p.setFont(smallFont);
    for (int i = 0; i <= 4; ++i) {
        const double v = yMin_ + (yMax_ - yMin_) * i / 4.0;
        p.setPen(QPen(v == 0 ? ui::Colors::axis() : ui::Colors::grid(), 1));
        p.drawLine(QPointF(r.left(), y(v)), QPointF(r.right(), y(v)));
        p.setPen(ui::Colors::muted());
        p.drawText(QRectF(0, y(v) - 7, 36, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', 1));
    }
    p.drawText(QRectF(r.left(), r.bottom() + 4, r.width(), 14), Qt::AlignRight, QString(T("last %1 s")).arg(span_, 0, 'f', 1));

    double legendX = r.right();
    for (auto it = series_.rbegin(); it != series_.rend(); ++it) {
        const auto& s = *it;
        const double w = p.fontMetrics().horizontalAdvance(s.name) + 24;
        legendX -= w;
        p.setPen(QPen(s.color, 2));
        p.drawLine(QPointF(legendX, 39), QPointF(legendX + 14, 39));
        p.setPen(ui::Colors::muted());
        p.drawText(QPointF(legendX + 18, 43), s.name);
        if (s.values.size() < 2) continue;
        QPainterPath path;
        const double n = static_cast<double>(s.values.size() - 1);
        for (size_t i = 0; i < s.values.size(); ++i) {
            const QPointF pt(r.left() + r.width() * static_cast<double>(i) / n, y(s.values[i]));
            if (i == 0) path.moveTo(pt);
            else path.lineTo(pt);
        }
        p.setPen(QPen(s.color, 1.6));
        p.drawPath(path);
    }
}
