#pragma once

#include <QMainWindow>

#include "stickyaim/platform/license.hpp"

class AppController;
class QLabel;
class QListWidget;
class QStackedWidget;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppController* app, QWidget* parent = nullptr);
    ~MainWindow() override;
    // Renders every page to <dir>/<n>_<name>.png after letting live data
    // accumulate, then quits. Used for documentation and UI verification.
    void captureAllPages(const QString& dir);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void registerHotkeys();
    void unregisterHotkeys();
    void showToast(const QString& text, int ms = 4000);  // transient message at the bottom of the window
    void placeToast();
    void refreshModePill();
    void checkLicense();  // every 30 s: locks the app when the code expires
    void lockApp();

    AppController* app_;
    QListWidget* nav_;
    QStackedWidget* stack_;
    QLabel* toast_;
    QTimer* toastTimer_;
    QLabel* modePill_;
    QLabel* licenseInfo_;  // remaining time of a test code, under the mode pill
    sa::win::license::Info license_;
    bool expiryWarned_ = false;
};
