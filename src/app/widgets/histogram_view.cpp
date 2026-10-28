#include "widgets/histogram_view.hpp"

#include <QPainter>

#include <algorithm>
#include <cmath>

#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

HistogramView::HistogramView(QString title, QString unit, QWidget* parent)
    : QWidget(parent), title_(std::move(title)), unit_(std::move(unit)) {
    setMinimumHeight(160);
}

void HistogramView::setHistogram(const sa::Histogram& h) {
    count_ = h.count();
    bars_.clear();
    if (count_ == 0) {
        update();
        return;
    }
    p50_ = h.percentile(50);
    p99_ = h.percentile(99);
    mean_ = h.mean();
    const double lo = std::max(0.0, h.percentile(0.1) * 0.7);
    const double hi = std::max(lo + h.binWidth() * 10, h.percentile(99.9) * 1.4);
    // Re-bin into ~80 display bars.
    const int nBars = 80;
    binWidth_ = (hi - lo) / nBars;
    x0_ = lo;
    bars_.assign(nBars, 0.0);
    const auto& bins = h.bins();
    for (size_t i = 0; i < bins.size(); ++i) {
        if (!bins[i]) continue;
        const double x = (static_cast<double>(i) + 0.5) * h.binWidth();
        if (x < lo || x >= hi) continue;
        bars_[std::min<size_t>(nBars - 1, static_cast<size_t>((x - lo) / binWidth_))] += bins[i];
    }
    update();
}

void HistogramView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    ui::paintCard(p, rect());
    // Title on the first line, statistics on the second, bars below.
    const QRectF r(18, 54, width() - 36, height() - 78);
    QFont titleFont = font();
    titleFont.setPixelSize(12);
    titleFont.setWeight(QFont::DemiBold);
    QFont smallFont = font();
    smallFont.setPixelSize(11);
    p.setFont(titleFont);
    p.setPen(ui::Colors::muted());
    p.drawText(QRectF(18, 12, width() - 36, 18), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(title_, Qt::ElideRight, width() - 36));
    p.setFont(smallFont);
    if (bars_.empty()) {
        p.drawText(r, Qt::AlignCenter, T("no data yet"));
        return;
    }
    p.setPen(ui::Colors::tertiary());
    p.drawText(QRectF(18, 30, width() - 36, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QString(T("n=%1 · mean %2 · p50 %3 · p99 %4 %5"))
                   .arg(count_)
                   .arg(mean_, 0, 'f', 1)
                   .arg(p50_, 0, 'f', 1)
                   .arg(p99_, 0, 'f', 1)
                   .arg(unit_));
    const double maxBar = *std::max_element(bars_.begin(), bars_.end());
    const double bw = r.width() / static_cast<double>(bars_.size());
    p.setPen(Qt::NoPen);
    p.setBrush(ui::Colors::hip());
    for (size_t i = 0; i < bars_.size(); ++i) {
        if (bars_[i] <= 0) continue;
        // sqrt scale keeps rare outliers visible
        const double h = std::sqrt(bars_[i] / maxBar) * r.height();
        p.drawRect(QRectF(r.left() + i * bw + 0.5, r.bottom() - h, std::max(1.0, bw - 1), h));
    }
    auto marker = [&](double v, QColor c, const QString& label) {
        const double x = r.left() + (v - x0_) / (binWidth_ * bars_.size()) * r.width();
        if (x < r.left() || x > r.right()) return;
        p.setPen(QPen(c, 1.5, Qt::DashLine));
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        p.setPen(c);
        p.drawText(QPointF(x + 3, r.top() + 12), label);
    };
    marker(p50_, ui::Colors::good(), "p50");
    marker(p99_, ui::Colors::warn(), "p99");
    p.setPen(ui::Colors::muted());
    p.drawText(QRectF(r.left(), r.bottom() + 4, 100, 14), Qt::AlignLeft, QString::number(x0_, 'f', 1));
    p.drawText(QRectF(r.right() - 220, r.bottom() + 4, 220, 14), Qt::AlignRight,
               QString(T("%1 %2 (sqrt scale)")).arg(x0_ + binWidth_ * bars_.size(), 0, 'f', 1).arg(unit_));
}
