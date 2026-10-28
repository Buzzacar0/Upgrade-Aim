#include "main_window.hpp"

#include <windows.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QTimer>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QComboBox>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QScrollArea>
#include <QIcon>
#include <QLabel>
#include <QListWidgetItem>
#include <QStyle>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "widgets/ui_helpers.hpp"
#include "launcher.hpp"
#include "license_ui.hpp"
#include "stickyaim/core/logger.hpp"

#include <functional>
#include <memory>
#include <vector>
#include "i18n.hpp"

namespace {

constexpr int kHotkeyBypass = 1;
constexpr int kHotkeySafeStop = 2;
constexpr int kHotkeyProfileBase = 10;  // +1..+9

}  // namespace

MainWindow::MainWindow(AppController* app, QWidget* parent) : QMainWindow(parent), app_(app) {
    setWindowTitle("Upgrade Aim");
    setWindowIcon(QIcon(":/ui/up-icon.png"));
    resize(1380, 900);
    setMinimumSize(1100, 720);

    auto* central = new QWidget;
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Sidebar: brand, navigation, current mode.
    auto* sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setFixedWidth(232);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(0, 22, 0, 18);
    side->setSpacing(0);
    auto* brandRow = new QHBoxLayout;
    brandRow->setContentsMargins(20, 0, 20, 0);
    brandRow->setSpacing(10);
    auto* mark = new QLabel;
    mark->setPixmap(QIcon(":/ui/up-icon.png").pixmap(28, 28));
    auto* brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto* brand = new QLabel("Upgrade Aim");
    brand->setObjectName("brand");
    auto* brandSub = new QLabel(T("Aim lab"));
    brandSub->setObjectName("brandSub");
    brandText->addWidget(brand);
    brandText->addWidget(brandSub);
    brandRow->addWidget(mark);
    brandRow->addLayout(brandText, 1);
    side->addLayout(brandRow);
    side->addSpacing(22);

    nav_ = new QListWidget;
    nav_->setObjectName("nav");
    nav_->setIconSize(QSize(18, 18));
    nav_->setFrameShape(QFrame::NoFrame);
    nav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    side->addWidget(nav_, 1);

    modePill_ = new QLabel;
    modePill_->setAlignment(Qt::AlignCenter);
    auto* pillRow = new QHBoxLayout;
    pillRow->setContentsMargins(18, 0, 18, 0);
    pillRow->addWidget(modePill_);
    side->addLayout(pillRow);
    licenseInfo_ = new QLabel;
    licenseInfo_->setAlignment(Qt::AlignCenter);
    licenseInfo_->setStyleSheet("font-size: 11px; color: #8E8E93; padding-top: 6px;");
    side->addWidget(licenseInfo_);
    layout->addWidget(sidebar);

    stack_ = new QStackedWidget;
    auto* pageHost = new QWidget;
    auto* hostL = new QVBoxLayout(pageHost);
    hostL->setContentsMargins(32, 26, 32, 22);
    hostL->addWidget(stack_);
    layout->addWidget(pageHost, 1);
    setCentralWidget(central);

    struct Page {
        QString name;
        const char* icon;
        QWidget* widget;
    };
    // Players see four simple pages. The labs, the profile editor, the detailed
    // compatibility tables and the tests are for development: UpgradeAim.exe --dev.
    std::vector<Page> pages = {
        {T("Dashboard"), ":/ui/nav-dashboard.svg", new DashboardPage(app_)},
        {T("Aim Profiles"), ":/ui/nav-profiles.svg", new AimLevelsPage(app_)},
        {T("Device Compatibility"), ":/ui/nav-compat.svg", new StatusPage(app_)},
    };
    if (QCoreApplication::arguments().contains("--dev")) {
        pages.push_back({T("Stick Lab"), ":/ui/nav-stick.svg", new StickLabPage(app_)});
        pages.push_back({T("Profile editor"), ":/ui/nav-profiles.svg", new ProfilesPage(app_)});
        pages.push_back({T("Latency Lab"), ":/ui/nav-latency.svg", new LatencyPage(app_)});
        pages.push_back({T("Compatibility details"), ":/ui/nav-compat.svg", new CompatPage(app_)});
        pages.push_back({T("Testing"), ":/ui/nav-testing.svg", new TestingPage(app_)});
    }
    pages.push_back({T("Settings"), ":/ui/nav-settings.svg", new SettingsPage(app_)});
    for (const auto& p : pages) {
        nav_->addItem(new QListWidgetItem(QIcon(p.icon), p.name));
        // Pages scroll when the window is shorter than their content, instead
        // of compressing widgets on top of each other. The Dashboard is laid
        // out to fit the window and stays unscrolled.
        if (qobject_cast<DashboardPage*>(p.widget)) {
            stack_->addWidget(p.widget);
            continue;
        }
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(p.widget);
        stack_->addWidget(scroll);
    }
    connect(nav_, &QListWidget::currentRowChanged, stack_, &QStackedWidget::setCurrentIndex);
    nav_->setCurrentRow(0);

    // Combo boxes size to the layout, not to their longest item, so narrow
    // columns do not overflow; they still grow when there is room.
    for (auto* combo : stack_->findChildren<QComboBox*>()) {
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(8);
    }

    // Messages appear as a toast instead of a permanent status bar.
    toast_ = new QLabel(central);
    toast_->setStyleSheet(
        "background: rgba(44,44,48,0.96); color: #F5F5F7; border: 1px solid #3A3A3F; border-radius: 12px;"
        " padding: 9px 16px; font-size: 13px;");
    toast_->hide();
    toastTimer_ = new QTimer(this);
    toastTimer_->setSingleShot(true);
    connect(toastTimer_, &QTimer::timeout, toast_, &QWidget::hide);
    connect(app_, &AppController::statusMessage, this, [this](const QString& m) { showToast(m, 6000); });

    auto* pillTimer = new QTimer(this);
    connect(pillTimer, &QTimer::timeout, this, &MainWindow::refreshModePill);
    pillTimer->start(250);
    connect(app_, &AppController::engineStateChanged, this, &MainWindow::refreshModePill);
    refreshModePill();

    // Icon in the notification area (next to the clock) while the app runs.
    tray_ = new QSystemTrayIcon(QIcon(":/ui/up-icon.png"), this);
    tray_->setToolTip("Upgrade Aim");
    auto* trayMenu = new QMenu(this);
    trayMenu->addAction(T("Open Upgrade Aim"), this, [this] {
        showNormal();
        raise();
        activateWindow();
    });
    trayMenu->addSeparator();
    trayMenu->addAction(T("Quit"), this, &QWidget::close);
    tray_->setContextMenu(trayMenu);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    tray_->show();

    license_ = sa::win::license::installedLicense();
    auto* licenseTimer = new QTimer(this);
    connect(licenseTimer, &QTimer::timeout, this, &MainWindow::checkLicense);
    licenseTimer->start(30000);

    ui::applyWindowChrome(this);
    registerHotkeys();
}

void MainWindow::checkLicense() {
    license_ = sa::win::license::installedLicense();
    if (!license_.valid) {
        lockApp();
        return;
    }
    if (license_.timed() && license_ui::minutesLeft(license_) <= 5 && !expiryWarned_) {
        expiryWarned_ = true;
        showToast(QString(T("Your test period ends in %1.")).arg(license_ui::remainingText(license_)), 10000);
    }
}

void MainWindow::lockApp() {
    // Same safety path as closing the app: centre the output and remove the
    // virtual controller, then ask for a new code on the start screen.
    app_->stopEngine();
    hide();
    sa::logInfo("app", "license no longer valid, app locked: " + license_.error);
    Launcher launcher;
    if (launcher.exec() != QDialog::Accepted) {
        qApp->quit();
        return;
    }
    license_ = sa::win::license::installedLicense();
    expiryWarned_ = false;
    app_->startEngine(nullptr);
    show();
}

void MainWindow::refreshModePill() {
    const auto c = app_->counters();
    const bool running = app_->engineRunning();
    const bool injecting = running && app_->output() == AppController::OutputKind::ViGEm;
    QString name, text;
    if (c.bypass && running) {
        name = "badgeBypass";
        text = T("Bypass");
    } else if (injecting) {
        name = "badgeInject";
        text = T("Virtual controller active");
    } else {
        name = "badgePreview";
        text = T("Safe preview");
    }
    if (modePill_->objectName() != name) {
        modePill_->setObjectName(name);
        modePill_->style()->unpolish(modePill_);
        modePill_->style()->polish(modePill_);
    }
    if (modePill_->text() != text) modePill_->setText(text);
    const QString lic = license_.valid && license_.timed() ? QString(T("Test: %1 left")).arg(license_ui::remainingText(license_)) : QString();
    if (licenseInfo_->text() != lic) licenseInfo_->setText(lic);
}

void MainWindow::showToast(const QString& text, int ms) {
    toast_->setText(text);
    toast_->adjustSize();
    placeToast();
    toast_->raise();
    toast_->show();
    toastTimer_->start(ms);
}

void MainWindow::placeToast() {
    if (!toast_) return;
    const QWidget* host = centralWidget();
    const int sidebarW = 232;
    const int x = sidebarW + (host->width() - sidebarW - toast_->width()) / 2;
    toast_->move(std::max(sidebarW + 8, x), host->height() - toast_->height() - 22);
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    placeToast();
}

MainWindow::~MainWindow() { unregisterHotkeys(); }

void MainWindow::registerHotkeys() {
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const UINT mods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
    if (!RegisterHotKey(hwnd, kHotkeyBypass, mods, 'P')) showToast(T("Ctrl+Alt+P is in use by another program."));
    if (!RegisterHotKey(hwnd, kHotkeySafeStop, mods, 'X')) showToast(T("Ctrl+Alt+X is in use by another program."));
    for (int i = 1; i <= 9; ++i) RegisterHotKey(hwnd, kHotkeyProfileBase + i, mods, '0' + i);
}

void MainWindow::unregisterHotkeys() {
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    UnregisterHotKey(hwnd, kHotkeyBypass);
    UnregisterHotKey(hwnd, kHotkeySafeStop);
    for (int i = 1; i <= 9; ++i) UnregisterHotKey(hwnd, kHotkeyProfileBase + i);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    auto* msg = static_cast<MSG*>(message);
    if (eventType == "windows_generic_MSG" && msg->message == WM_HOTKEY) {
        const int id = static_cast<int>(msg->wParam);
        if (id == kHotkeyBypass) {
            app_->setBypass(!app_->bypass());
            showToast(app_->bypass() ? T("Pass-through ON: raw controller input") : T("Pass-through OFF: profile active"), 5000);
        } else if (id == kHotkeySafeStop) {
            // Safe shutdown: remove any virtual controller and fall back to preview.
            app_->setOutput(AppController::OutputKind::Preview, nullptr);
            showToast(T("Safe stop: virtual output removed, preview mode."), 8000);
        } else if (id > kHotkeyProfileBase && id <= kHotkeyProfileBase + 9) {
            const auto lib = app_->profileLibrary();
            const int idx = id - kHotkeyProfileBase - 1;
            if (idx < lib.size()) {
                QString err;
                if (app_->loadProfileFile(lib[idx], &err)) showToast(T("Profile: ") + QString::fromStdString(app_->profile().name), 4000);
            }
        }
        *result = 0;
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::captureAllPages(const QString& dir) {
    QDir().mkpath(dir);
    auto step = std::make_shared<std::function<void(int)>>();
    *step = [this, dir, step](int i) {
        if (i >= stack_->count()) {
            close();
            return;
        }
        nav_->setCurrentRow(i);
        // Exercise the Testing page through its real button.
        if (auto* run = stack_->widget(i)->findChild<QPushButton*>("runSynthetic")) run->click();
        QTimer::singleShot(1500, this, [this, dir, step, i] {
            QString name = nav_->item(i)->text().toLower().replace(' ', '_');
            grab().save(QString("%1/%2_%3.png").arg(dir).arg(i + 1).arg(name));
            (*step)(i + 1);
        });
    };
    QTimer::singleShot(2500, this, [step] { (*step)(0); });
}

void MainWindow::closeEvent(QCloseEvent* e) {
    app_->stopEngine();  // neutralizes and removes any virtual controller
    QMainWindow::closeEvent(e);
}
