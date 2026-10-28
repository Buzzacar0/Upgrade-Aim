#pragma once

#include <QWidget>

#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "stickyaim/core/analysis.hpp"
#include "stickyaim/core/calibration.hpp"

class AppController;
class ContextEditor;
class ShapeEditor;
class CurveEditor;
class HistogramView;
class StickView;
class TimePlot;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;

// 1. Dashboard — device, profile, engine/output state.
class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(AppController* app, QWidget* parent = nullptr);

private:
    void refresh();
    void refreshProfiles();
    void refreshResponse(bool running, bool bypass, bool injecting);
    AppController* app_;
    QLabel *badge_, *device_, *source_, *engine_, *profile_, *context_, *rate_, *counters_, *drift_;
    QComboBox *sourceCombo_, *profileCombo_;
    QPushButton* startStop_;
    QCheckBox* active_;     // Active (profile on) / Bypass switch
    QLabel* activeLabel_;
    StickView *left_, *right_;
    // Read rate and response time tiles: 0 = rate (Hz), 1 = without app, 2 = with app, 3 = smoothing.
    QLabel* tileValue_[4];
    QLabel* tileUnit_[4];
    QLabel* tileSub_[4];
};

// 2. Stick Lab — live visualization, curve editor, calibration.
class StickLabPage : public QWidget {
    Q_OBJECT
public:
    explicit StickLabPage(AppController* app, QWidget* parent = nullptr);

private:
    void onTelemetry();
    void onProfile();
    void runCalibration(bool rest);
    void finishCalibration(bool rest);
    AppController* app_;
    StickView *leftView_, *rightView_;
    CurveEditor* curve_;
    TimePlot* plot_;
    QComboBox *editContext_, *editAxis_, *view_;
    QLabel *calStatus_, *contextLabel_;
    QPushButton *restBtn_, *rangeBtn_, *applyCenter_, *applyDeadzone_, *applyRange_;
    ShapeEditor* shapeEdit_;
    QDoubleSpinBox *inner_, *outer_;
    sa::RestReport lastRest_{};
    sa::RangeReport lastRange_{};
    sa::StickCalibration lastRangeCal_{};
    bool updating_ = false;
};

// 3. Aim Profiles — hip / ADS contexts, fine & fast bands, profile files.
class ProfilesPage : public QWidget {
    Q_OBJECT
public:
    explicit ProfilesPage(AppController* app, QWidget* parent = nullptr);

private:
    void load();
    void pushToController();
    void refreshList();
    AppController* app_;
    QListWidget* list_;
    ContextEditor *hip_, *ads_;
    QDoubleSpinBox *adsEnter_, *adsExit_, *transition_, *moveInner_, *moveOuter_, *ltInner_, *rtInner_;
    QCheckBox* adsFromLt_;
    QLabel *errors_, *fileLabel_;
    QPlainTextEdit* description_;
    bool updating_ = false;
};

// 4. Latency Lab — sample intervals, jitter, processing time, HID report rate.
class LatencyPage : public QWidget {
    Q_OBJECT
public:
    explicit LatencyPage(AppController* app, QWidget* parent = nullptr);

private:
    void refresh();
    AppController* app_;
    HistogramView *intervals_, *process_, *total_;
    QLabel *summary_, *engineStats_;
    QTableWidget* hid_;
    QComboBox* mode_;
    QSpinBox* interval_;
    QCheckBox* mmcss_;
};

// 5. Device Compatibility — controllers, drivers, launchers, manual checklist.
class CompatPage : public QWidget {
    Q_OBJECT
public:
    explicit CompatPage(AppController* app, QWidget* parent = nullptr);

private:
    void refresh();
    void saveResults();
    QString buildReport() const;
    AppController* app_;
    QTableWidget *xinput_, *hid_, *drivers_, *launchers_, *checklist_;
    QLabel* summary_;
};

// 6. Testing — repeatable synthetic and live comparisons.
class TestingPage : public QWidget {
    Q_OBJECT
public:
    explicit TestingPage(AppController* app, QWidget* parent = nullptr);

private:
    void runSynthetic();
    void startLive(int seconds);
    void finishLive();
    void showComparison(const std::vector<std::pair<QString, sa::ComparisonReport>>& reps);
    void exportReport();
    AppController* app_;
    QComboBox *baseline_, *context_, *liveKind_;
    QTableWidget* table_;
    QPlainTextEdit* notes_;
    QLabel *status_, *liveStatus_;
    QPushButton *liveBtn_;
    nlohmann::json lastReport_;
    QString liveName_;
};

// 7. Settings — virtual output opt-in, polling, panic combo, import/export/reset.
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(AppController* app, QWidget* parent = nullptr);

private:
    void refresh();
    AppController* app_;
    QCheckBox *allowVirtual_, *panicEnabled_, *forwardRumble_;
    QSpinBox* panicHold_;
    QComboBox* output_;
    QLabel* outputStatus_;
};
