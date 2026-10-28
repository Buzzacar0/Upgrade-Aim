#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <deque>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "stickyaim/core/device_db.hpp"
#include "stickyaim/core/pipeline.hpp"
#include "stickyaim/core/stats.hpp"
#include "stickyaim/engine/engine.hpp"
#include "stickyaim/platform/rawinput.hpp"

// Owns the engine, the current profile and calibration, the telemetry history
// shown by the pages, and persistent settings. Lives on the UI thread.
class AppController : public QObject {
    Q_OBJECT
public:
    enum class SourceKind { XInput, Hid, Demo };
    struct SourceSel {
        SourceKind kind = SourceKind::Demo;
        int slot = 0;
        QString hidPath;
        QString label;
    };
    enum class OutputKind { Preview, ViGEm };

    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    // ---- profile & calibration
    const sa::Profile& profile() const { return profile_; }
    void setProfile(const sa::Profile& p);  // validates, compiles (debounced), publishes
    QStringList profileErrors() const { return errors_; }
    const sa::DeviceCalibration& calibration() const { return calibration_; }
    // Right-stick deadzone the player set in the game (0-40 %). The engine
    // compensates it with anti-deadzone; see effectiveProfile().
    int gameDeadzonePercent() const;
    void setGameDeadzonePercent(int percent);
    sa::Profile effectiveProfile() const;
    void setCalibration(const sa::DeviceCalibration& c);
    QString profilePath() const { return profilePath_; }
    bool profileDirty() const { return dirty_; }
    bool loadProfileFile(const QString& path, QString* error);
    bool saveProfileFile(const QString& path, QString* error);
    QStringList profileLibrary() const;  // shipped examples + user profiles
    QString userProfilesDir() const;
    QString userDataDir() const;

    // ---- engine
    bool startEngine(QString* error);
    void stopEngine();
    bool engineRunning() const;
    SourceSel source() const { return source_; }
    void setSource(const SourceSel& s);
    OutputKind output() const { return output_; }
    // Switching to ViGEm requires allowVirtualOutput() in settings.
    bool setOutput(OutputKind kind, QString* error);
    void setBypass(bool on);
    bool bypass() const;
    sa::EngineCounters counters() const;
    sa::EngineConfig engineConfig() const { return engineCfg_; }
    void setEngineConfig(const sa::EngineConfig& cfg);  // restarts the engine if running
    QString sourceDescription() const;
    // Hides the physical controller from games with HidHide (asks Windows for
    // administrator rights). Returns a message for the user; *ok tells success.
    QString setupHidHide(bool undo, bool* ok);
    QString matchedDeviceName() const;  // cached; see refreshDeviceMatch()
    void refreshDeviceMatch();          // re-enumerates HID devices (slow: call on changes only)

    // ---- telemetry (UI-thread copies)
    const std::deque<sa::TelemetrySample>& recent() const { return recent_; }
    bool hasLatest() const { return !recent_.empty(); }
    const sa::TelemetrySample& latest() const { return recent_.back(); }
    double newSampleRateHz() const { return rateHz_; }

    // Live read-rate and response-time figures for the Dashboard, updated every 0.5 s.
    struct ResponseStats {
        bool deviceRateValid = false;  // source reports a device sequence (XInput packet number, HID report count)
        double deviceHz = 0;           // state changes / reports the controller produced per second
        double appHz = 0;              // of those, new states the engine read and processed per second
        double deviceIntervalMs = 0;   // 1 / deviceHz: how often new data exists, with or without the app
        double addedMeanMs = 0;        // app path: mean (poll gap / 2 + read -> processed -> submitted)
        double addedP99Ms = 0;         // app path worst case: poll gap + read -> submitted, 99th percentile
        double smoothingHipMs = 0;     // lag added by the active profile's smoothing (synthetic 1 Hz tracking)
        double smoothingAdsMs = 0;
        // What the screen shows: UI update rate and how old the newest drawn sample is.
        double uiFps = 0;
        double uiWorstFrameMs = 0;     // longest gap between UI updates in the window
        double displayAgeMs = 0;       // mean (time of UI update - acquisition time of newest sample)
    };
    const ResponseStats& responseStats() const { return response_; }
    const sa::TimingAnalyzer& sampleTiming() const { return sampleTiming_; }
    const sa::Histogram& processHist() const { return processHist_; }
    const sa::Histogram& submitHist() const { return submitHist_; }
    const sa::Histogram& totalHist() const { return totalHist_; }
    void resetLatencyStats();

    void beginCapture();
    std::vector<sa::RawSample> endCapture();
    bool capturing() const { return capturing_; }
    double captureSeconds() const;

    std::shared_ptr<sa::win::RawInputMonitor> rawInput() const { return rawInput_; }
    const std::vector<sa::KnownDevice>& deviceDb() const { return deviceDb_; }

    // ---- settings (persisted to %APPDATA%\UpgradeAim\settings.json)
    nlohmann::json& settings() { return settings_; }
    void saveSettings();
    bool allowVirtualOutput() const;
    void resetAllSettings();

Q_SIGNALS:
    void profileChanged();
    void calibrationChanged();
    void telemetryTick();
    void engineStateChanged();
    void statusMessage(const QString& msg);

private Q_SLOTS:
    void drainTelemetry();
    void publishCompiled();

private:
    std::unique_ptr<sa::IInputSource> makeSource() const;
    void loadSettings();
    void applySettingsToEngineConfig();
    void restartIfRunning();
    void shutdownEngine();  // stops the engine without touching HidHide
    void syncHidHide();
    void measureSmoothingLag(const sa::CompiledProfile& cp);
    void resetResponseWindow();

    sa::Profile profile_;
    sa::DeviceCalibration calibration_;
    QString profilePath_;
    QStringList errors_;
    bool dirty_ = false;

    SourceSel source_;
    OutputKind output_ = OutputKind::Preview;
    sa::EngineConfig engineCfg_;
    std::unique_ptr<sa::Engine> engine_;
    bool hidHidden_ = false;  // this app hid the physical controller with HidHide
    std::shared_ptr<sa::win::RawInputMonitor> rawInput_;
    std::vector<sa::KnownDevice> deviceDb_;
    QString matchedName_;

    QTimer drainTimer_;
    QTimer publishTimer_;
    std::deque<sa::TelemetrySample> recent_;
    sa::TimingAnalyzer sampleTiming_{3.0, 1.0, 50000};
    sa::Histogram processHist_{0.01, 100000};  // us
    sa::Histogram submitHist_{0.1, 100000};    // us
    sa::Histogram totalHist_{0.1, 100000};     // us
    double rateHz_ = 0;
    std::uint64_t rateCount_ = 0;
    sa::TimestampNs rateWindowStart_ = 0;
    ResponseStats response_;
    bool haveSeq_ = false;
    std::uint32_t lastSeq_ = 0;
    std::uint64_t seqCount_ = 0;     // device sequence advance in the current window
    double addedSumUs_ = 0;          // sum of mean-estimate app latency in the current window
    std::uint64_t addedCount_ = 0;
    sa::Histogram addedWorstHist_{0.5, 4000};  // us, current window
    sa::TimestampNs lastDrain_ = 0;
    sa::TimestampNs uiWindowStart_ = 0;  // first UI update of the current window
    std::uint64_t uiFrames_ = 0;
    double uiWorstGapMs_ = 0, ageSumMs_ = 0;
    sa::TimestampNs lastUiLog_ = 0;

    bool capturing_ = false;
    std::vector<sa::RawSample> capture_;

    nlohmann::json settings_;
};