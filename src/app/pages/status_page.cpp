#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "stickyaim/platform/system_probe.hpp"
#include "stickyaim/platform/xinput_source.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

namespace {

enum class State { Ok, Warn, Bad, Off };

QColor stateColor(State s) {
    switch (s) {
        case State::Ok: return ui::Colors::good();
        case State::Warn: return ui::Colors::warn();
        case State::Bad: return ui::Colors::bad();
        default: return ui::Colors::tertiary();
    }
}

QString rgba(const QColor& c, double a) {
    return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
}

// Round mark: check, exclamation, cross or dash on a tinted circle.
QLabel* mark(State s, int size) {
    static const char* glyph[] = {"✓", "!", "✕", "–"};
    auto* l = new QLabel(QString::fromUtf8(glyph[static_cast<int>(s)]));
    const QColor c = stateColor(s);
    l->setFixedSize(size, size);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QString("background: %1; color: %2; border-radius: %3px; font-size: %4px; font-weight: 700;")
                         .arg(rgba(c, 0.16), c.name())
                         .arg(size / 2)
                         .arg(size / 2));
    return l;
}

QFrame* statusCard(State s, const QString& title, const QString& value, const QString& text) {
    QVBoxLayout* outer = nullptr;
    auto* card = ui::card(&outer, 20);
    auto* row = new QHBoxLayout;
    row->setSpacing(16);
    row->addWidget(mark(s, 52), 0, Qt::AlignTop);
    auto* col = new QVBoxLayout;
    col->setSpacing(3);
    auto* t = new QLabel(title);
    t->setStyleSheet("font-size: 17px; font-weight: 600; color: #F5F5F7;");
    auto* v = new QLabel(value);
    v->setStyleSheet(QString("font-size: 15px; font-weight: 600; color: %1;").arg(stateColor(s) == ui::Colors::tertiary() ? "#98989D" : stateColor(s).name()));
    col->addWidget(t);
    col->addWidget(v);
    if (!text.isEmpty()) {
        auto* n = ui::note(text);
        n->setStyleSheet("font-size: 13px;");
        col->addWidget(n);
    }
    col->addStretch();
    row->addLayout(col, 1);
    outer->addLayout(row);
    return card;
}

}  // namespace

StatusPage::StatusPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(22);
    auto* top = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::heading(T("Device Compatibility")));
    titles->addWidget(ui::subheading(T("Green is ready. Yellow or red shows what is missing and what to do.")));
    top->addLayout(titles, 1);
    auto* again = new QPushButton(T("Check again"));
    again->setProperty("primary", true);
    top->addWidget(again, 0, Qt::AlignTop);
    page->addLayout(top);

    grid_ = new QWidget;
    page->addWidget(grid_);
    page->addStretch();
    connect(again, &QPushButton::clicked, this, &StatusPage::refresh);
    connect(app_, &AppController::engineStateChanged, this, [this] {
        if (isVisible()) refresh();
    });
}

void StatusPage::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh();
}

void StatusPage::refresh() {
    // Rebuild the cards from scratch: there are only a few.
    delete grid_->layout();
    for (auto* child : grid_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) delete child;
    auto* v = new QVBoxLayout(grid_);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(22);

    const auto drv = sa::win::probeDrivers();
    bool padConnected = false;
    for (const auto& slot : sa::win::XInputSource::querySlots()) padConnected = padConnected || slot.connected;
    const bool injecting = app_->engineRunning() && app_->output() == AppController::OutputKind::ViGEm;

    // Overall answer first.
    const bool ready = padConnected && drv.vigemBus.installed && injecting;
    const State overall = ready ? State::Ok : (padConnected ? State::Warn : State::Bad);
    {
        QVBoxLayout* l = nullptr;
        auto* banner = ui::card(&l, 22);
        auto* row = new QHBoxLayout;
        row->setSpacing(18);
        row->addWidget(mark(overall, 64));
        auto* text = new QLabel(ready ? T("All set to play") : (padConnected ? T("Almost there") : T("Connect your controller")));
        text->setStyleSheet(QString("font-size: 26px; font-weight: 700; color: %1;").arg(stateColor(overall).name()));
        auto* sub = ui::note(ready ? T("The game is receiving the Upgrade Aim controller.")
                                   : T("Look at the cards below: the yellow or red one says what to do."));
        sub->setStyleSheet("font-size: 14px;");
        auto* col = new QVBoxLayout;
        col->setSpacing(4);
        col->addWidget(text);
        col->addWidget(sub);
        row->addLayout(col, 1);
        l->addLayout(row);
        v->addWidget(banner);
    }

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(18);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    const QString name = app_->matchedDeviceName();
    grid->addWidget(padConnected ? statusCard(State::Ok, T("Controller"), T("Connected"), name.isEmpty() ? QString() : name)
                                 : statusCard(State::Bad, T("Controller"), T("Not found"), T("Plug in the cable or the receiver and press Check again.")),
                    0, 0);
    grid->addWidget(drv.vigemBus.installed
                        ? statusCard(State::Ok, T("Virtual controller (ViGEmBus)"), T("Installed"), T("Carries the Upgrade Aim aim to the game."))
                        : statusCard(State::Bad, T("Virtual controller (ViGEmBus)"), T("Not installed"),
                                     T("Run the Upgrade Aim installer again and leave ViGEmBus ticked.")),
                    0, 1);
    grid->addWidget(drv.hidHide.installed
                        ? statusCard(State::Ok, T("Hide the controller (HidHide)"), T("Installed"), T("Makes the game see only the Upgrade Aim controller."))
                        : statusCard(State::Warn, T("Hide the controller (HidHide)"), T("Not installed"),
                                     T("Without it the game may see two controllers. Run the installer again and leave HidHide ticked.")),
                    1, 0);
    if (injecting)
        grid->addWidget(statusCard(State::Ok, T("Output to the game"), T("On now"), T("The game is receiving the Upgrade Aim controller.")), 1, 1);
    else if (app_->allowVirtualOutput())
        grid->addWidget(statusCard(State::Warn, T("Output to the game"), T("Off"), T("Turn on the virtual controller in Settings.")), 1, 1);
    else
        grid->addWidget(statusCard(State::Warn, T("Output to the game"), T("Off"),
                                   T("Allow the virtual controller in Settings so the game gets the Upgrade Aim aim.")),
                        1, 1);
    v->addLayout(grid);

    // Launchers: just whether each one is on this PC.
    auto* header = new QLabel(T("Launchers"));
    header->setObjectName("groupHeader");
    v->addWidget(header);
    auto* launchers = new QHBoxLayout;
    launchers->setSpacing(18);
    for (const auto& l : sa::win::probeLaunchers()) {
        const QString value = !l.installed ? T("Not found") : (l.running ? T("Installed · open now") : T("Installed"));
        launchers->addWidget(statusCard(l.installed ? State::Ok : State::Off, T(l.name.c_str()), value, {}), 1);
    }
    v->addLayout(launchers);
}
