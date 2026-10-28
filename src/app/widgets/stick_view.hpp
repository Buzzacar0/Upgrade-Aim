#pragma once

#include <QWidget>

#include <vector>

#include "stickyaim/core/types.hpp"

// Live 2D stick view: unit circle, deadzone rings, a fading trail of raw
// (grey) and processed (coloured) positions, and the current points.
class StickView : public QWidget {
    Q_OBJECT
public:
    explicit StickView(QString title, QWidget* parent = nullptr);
    void setPoints(std::vector<sa::Vec2> raw, std::vector<sa::Vec2> processed);
    void setRings(float inner, float outer) {
        inner_ = inner;
        outer_ = outer;
        update();
    }
    void setProcessedColor(QColor c) {
        procColor_ = c;
        update();
    }
    QSize sizeHint() const override { return {260, 280}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w + 20; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString title_;
    std::vector<sa::Vec2> raw_, proc_;
    float inner_ = 0, outer_ = 1;
    QColor procColor_;
};
