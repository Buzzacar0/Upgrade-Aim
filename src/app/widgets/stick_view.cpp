#include "widgets/stick_view.hpp"

#include <QPainter>
#include <QPainterPath>

#include "i18n.hpp"
#include "widgets/ui_helpers.hpp"

StickView::StickView(QString title, QWidget* parent) : QWidget(parent), title_(std::move(title)), procColor_(ui::Colors::hip()) {
    setMinimumSize(200, 220);
}

void StickView::setPoints(std::vector<sa::Vec2> raw, std::vector<sa::Vec2> processed) {
    raw_ = std::move(raw);
    proc_ = std::move(processed);
    update();
}

void StickView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    ui::paintCard(p, rect());

    const int pad = 18;
    const int headerH = 22;
    const int footerH = 20;
    QFont titleFont = font();
    titleFont.setPixelSize(12);
    titleFont.setWeight(QFont::DemiBold);
    QFont smallFont = font();
    smallFont.setPixelSize(11);

    // Title (left) and legend (right).
    p.setFont(titleFont);
    p.setPen(ui::Colors::muted());
    p.drawText(QRect(pad, pad - 4, width() - 2 * pad, headerH), Qt::AlignLeft | Qt::AlignVCenter, title_);
    p.setFont(smallFont);
    // Legend only when it fits beside the title.
    const int legendW = (proc_.empty() ? 0 : p.fontMetrics().horizontalAdvance(T("Processed")) + 26) +
                        (raw_.empty() ? 0 : p.fontMetrics().horizontalAdvance(T("Raw")) + 26);
    const bool showLegend = QFontMetrics(titleFont).horizontalAdvance(title_) + legendW + 12 <= width() - 2 * pad;
    int lx = width() - pad;
    auto legend = [&](const QString& text, QColor col) {
        const int tw = p.fontMetrics().horizontalAdvance(text);
        lx -= tw;
        p.setPen(ui::Colors::muted());
        p.drawText(QRect(lx, pad - 4, tw, headerH), Qt::AlignVCenter, text);
        lx -= 12;
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPointF(lx + 4, pad - 4 + headerH / 2.0), 3.5, 3.5);
        lx -= 14;
    };
    if (showLegend && !proc_.empty()) legend(T("Processed"), procColor_);
    if (showLegend && !raw_.empty()) legend(T("Raw"), ui::Colors::raw());

    const int side = std::min(width() - 2 * pad, height() - pad * 2 - headerH - footerH);
    const QPointF c(width() / 2.0, pad + headerH + 4 + side / 2.0);
    const double r = side / 2.0;
    auto map = [&](sa::Vec2 v) { return QPointF(c.x() + v.x * r, c.y() - v.y * r); };

    // Field: faint fill, crosshair, half-deflection ring, rim, deadzone rings.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 6));
    p.drawEllipse(c, r, r);
    p.setPen(QPen(ui::Colors::grid(), 1));
    p.drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - r), QPointF(c.x(), c.y() + r));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 34), 1));
    p.drawEllipse(c, r * 0.5, r * 0.5);
    p.setPen(QPen(QColor(255, 255, 255, 60), 1.2));
    p.drawEllipse(c, r, r);
    if (inner_ > 0) {
        p.setPen(QPen(QColor(255, 214, 10, 150), 1, Qt::DashLine));
        p.drawEllipse(c, r * inner_, r * inner_);
    }
    if (outer_ < 1) {
        p.setPen(QPen(QColor(48, 209, 88, 140), 1, Qt::DashLine));
        p.drawEllipse(c, r * outer_, r * outer_);
    }

    auto trail = [&](const std::vector<sa::Vec2>& pts, QColor col) {
        const size_t n = pts.size();
        for (size_t i = 1; i < n; ++i) {
            QColor cc = col;
            cc.setAlphaF(static_cast<float>(0.05 + 0.55 * static_cast<double>(i) / n));
            p.setPen(QPen(cc, 2.0, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(map(pts[i - 1]), map(pts[i]));
        }
    };
    trail(raw_, ui::Colors::raw());
    trail(proc_, procColor_);

    auto dot = [&](sa::Vec2 v, QColor col, double rad) {
        QColor glow = col;
        glow.setAlpha(60);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(map(v), rad + 4, rad + 4);
        p.setBrush(col);
        p.drawEllipse(map(v), rad, rad);
    };
    if (!raw_.empty()) dot(raw_.back(), ui::Colors::raw(), 4.5);
    if (!proc_.empty()) dot(proc_.back(), procColor_, 5.5);

    p.setFont(smallFont);
    const QRect footer(pad, height() - pad - footerH + 6, width() - 2 * pad, footerH);
    if (!raw_.empty()) {
        p.setPen(ui::Colors::tertiary());
        const auto rv = raw_.back();
        p.drawText(footer, Qt::AlignLeft | Qt::AlignVCenter, QString("%1, %2").arg(rv.x, 6, 'f', 3).arg(rv.y, 6, 'f', 3));
    }
    if (!proc_.empty()) {
        p.setPen(procColor_);
        const auto pv = proc_.back();
        p.drawText(footer, Qt::AlignRight | Qt::AlignVCenter, QString("%1, %2").arg(pv.x, 6, 'f', 3).arg(pv.y, 6, 'f', 3));
    }
}
