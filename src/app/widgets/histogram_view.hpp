#pragma once

#include <QWidget>

#include "stickyaim/core/stats.hpp"

// Bar chart of a Histogram with p50 / p99 markers. The visible x-range adapts
// to the data (up to the 99.9th percentile x 1.4).
class HistogramView : public QWidget {
    Q_OBJECT
public:
    explicit HistogramView(QString title, QString unit, QWidget* parent = nullptr);
    void setHistogram(const sa::Histogram& h);
    QSize sizeHint() const override { return {420, 200}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString title_, unit_;
    std::vector<double> bars_;
    double binWidth_ = 1, x0_ = 0;
    double p50_ = 0, p99_ = 0, mean_ = 0;
    std::uint64_t count_ = 0;
};
