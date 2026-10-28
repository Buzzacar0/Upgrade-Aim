#pragma once

#include <QWidget>

#include <vector>

// Scrolling time-series plot of up to four series on a fixed Y range.
class TimePlot : public QWidget {
    Q_OBJECT
public:
    struct Series {
        QString name;
        QColor color;
        std::vector<double> values;  // oldest first, uniformly spaced
    };
    explicit TimePlot(QString title, double yMin, double yMax, QWidget* parent = nullptr);
    void setSeries(std::vector<Series> s, double spanSeconds);
    QSize sizeHint() const override { return {520, 180}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString title_;
    double yMin_, yMax_;
    double span_ = 2;
    std::vector<Series> series_;
};
