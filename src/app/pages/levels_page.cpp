#include <QAbstractButton>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

struct Level {
    const char* title;
    const char* text;
    const char* file;  // base name in the profile library
    QColor color;
    int strength;      // 1..3 bars
};

const Level kLevels[] = {
    {"LOW", "Light aim. Almost the same as playing without the app.", "baixo", ui::Colors::good(), 1},
    {"MEDIUM", "Balanced. The aim feels steadier near the enemy without losing speed.", "medio", ui::Colors::warn(), 2},
    {"HIGH", "Strongest. The aim slows down near the centre so it sticks to the enemy more.", "upgrade", ui::Colors::bad(), 3},
};

}  // namespace

// One big clickable card: level name, strength bars, one plain sentence and
// an "in use" mark. Painted by hand so it reads at a glance.
class LevelCard : public QAbstractButton {
public:
    LevelCard(const Level& level, QWidget* parent = nullptr) : QAbstractButton(parent), level_(level) {
        setCursor(Qt::PointingHandCursor);
        setMinimumWidth(200);
        setFixedHeight(390);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAttribute(Qt::WA_Hover);
        setAccessibleName(T(level.title));
    }
    QString path;
    void setInUse(bool on) {
        if (inUse_ == on) return;
        inUse_ = on;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor c = level_.color;
        const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
        QPainterPath shape;
        shape.addRoundedRect(r, 20, 20);

        p.fillPath(shape, underMouse() && isEnabled() ? QColor(0x24, 0x24, 0x28) : ui::Colors::panel());
        if (inUse_) {
            QLinearGradient glow(r.topLeft(), r.bottomLeft());
            QColor top = c;
            top.setAlphaF(0.20f);
            QColor bottom = c;
            bottom.setAlphaF(0.02f);
            glow.setColorAt(0, top);
            glow.setColorAt(0.6, bottom);
            p.fillPath(shape, glow);
        }
        p.setPen(inUse_ ? QPen(c, 3) : QPen(QColor(0x2e, 0x2e, 0x33), 1.2));
        p.drawPath(shape);
        if (!isEnabled()) p.setOpacity(0.4);

        // Level name.
        QFont f(ui::fontFamily());
        f.setPixelSize(qBound(30, width() / 6, 46));
        f.setWeight(QFont::Bold);
        f.setLetterSpacing(QFont::PercentageSpacing, 104);
        p.setFont(f);
        p.setPen(c);
        const QRectF titleRect(r.left(), r.top() + r.height() * 0.10, r.width(), r.height() * 0.22);
        p.drawText(titleRect, Qt::AlignCenter, T(level_.title));

        // Strength bars.
        const double barW = 34, barH = 10, gap = 8;
        const double total = 3 * barW + 2 * gap;
        double x = r.center().x() - total / 2;
        const double y = titleRect.bottom() + 6;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            p.setBrush(i < level_.strength ? c : QColor(0x3a, 0x3a, 0x3f));
            p.drawRoundedRect(QRectF(x, y, barW, barH), 5, 5);
            x += barW + gap;
        }

        // Plain sentence.
        QFont body(ui::fontFamily());
        body.setPixelSize(17);
        p.setFont(body);
        p.setPen(QColor(0xd1, 0xd1, 0xd6));
        const QRectF textRect(r.left() + 22, y + barH + 22, r.width() - 44, r.height() * 0.32);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, T(level_.text));

        // In use / click to use.
        QFont pillFont(ui::fontFamily());
        pillFont.setPixelSize(15);
        pillFont.setWeight(QFont::DemiBold);
        p.setFont(pillFont);
        const QRectF pill(r.center().x() - 80, r.bottom() - 66, 160, 42);
        if (inUse_) {
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(pill, 21, 21);
            p.setPen(QColor(0x10, 0x10, 0x12));
            p.drawText(pill, Qt::AlignCenter, QString::fromUtf8("✓  ") + T("IN USE"));
        } else {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0x48, 0x48, 0x4a), 1.2));
            p.drawRoundedRect(pill, 21, 21);
            p.setPen(underMouse() ? ui::Colors::text() : ui::Colors::muted());
            p.drawText(pill, Qt::AlignCenter, T("Click to use"));
        }
    }

private:
    Level level_;
    bool inUse_ = false;
};

AimLevelsPage::AimLevelsPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(22);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::heading(T("Aim Profiles")));
    titles->addWidget(ui::subheading(T("Click a level. It changes right away.")));
    page->addLayout(titles);

    QStringList library = app_->profileLibrary();
    auto* row = new QHBoxLayout;
    row->setSpacing(18);
    for (const auto& level : kLevels) {
        auto* card = new LevelCard(level);
        for (const auto& path : library)
            if (QFileInfo(path).completeBaseName() == level.file) card->path = path;
        card->setEnabled(!card->path.isEmpty());
        connect(card, &QAbstractButton::clicked, this, [this, card] {
            if (QFileInfo(card->path) == QFileInfo(app_->profilePath()) && !app_->profileDirty()) return;
            QString err;
            if (!app_->loadProfileFile(card->path, &err)) QMessageBox::warning(this, T("Aim Profiles"), err);
        });
        cards_.push_back(card);
        row->addWidget(card, 1);
    }
    page->addSpacing(8);
    page->addLayout(row);

    status_ = new QLabel;
    status_->setTextFormat(Qt::RichText);
    status_->setWordWrap(true);
    status_->setAlignment(Qt::AlignCenter);
    status_->setStyleSheet("font-size: 17px; color: #F5F5F7;");
    page->addWidget(status_);
    auto* tip = ui::note(T("Tip: start on Medium. If the aim feels heavy, go down to Low; if you want it to stick more, go up to High."));
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("font-size: 14px;");
    page->addWidget(tip);
    page->addStretch();

    connect(app_, &AppController::profileChanged, this, &AimLevelsPage::refresh);
    connect(app_, &AppController::engineStateChanged, this, &AimLevelsPage::refresh);
    refresh();
}

void AimLevelsPage::refresh() {
    QString current;
    for (size_t i = 0; i < cards_.size(); ++i) {
        auto* card = cards_[i];
        const bool on = !card->path.isEmpty() && QFileInfo(card->path) == QFileInfo(app_->profilePath());
        card->setInUse(on);
        if (on) current = QString("<b style='color:%1'>%2</b>").arg(kLevels[i].color.name(), T(kLevels[i].title));
    }
    QString text = current.isEmpty() ? T("None of the three levels is in use right now.")
                                     : QString(T("In use now: %1")).arg(current);
    if (app_->engineRunning() && app_->bypass())
        text += "<br>" + T("Bypass is on in the Dashboard: the game gets the controller without the level.");
    status_->setText(text);
}
