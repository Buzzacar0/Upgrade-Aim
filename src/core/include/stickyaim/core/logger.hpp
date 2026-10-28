#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "stickyaim/core/spsc_queue.hpp"
#include "stickyaim/core/types.hpp"

namespace sa {

enum class LogLevel : std::uint8_t { Debug, Info, Warn, Error };

// Structured, timestamped logger writing JSON Lines. Writing happens on a
// background thread.
//
//  * logRt()  — for the real-time input thread only (single producer). Never
//               blocks or allocates; if the ring is full the record is dropped
//               and counted.
//  * log()    — for any other thread. Takes a mutex, never touches the file.
class Logger {
public:
    static Logger& instance();

    bool start(const std::string& path, LogLevel minLevel = LogLevel::Info);
    void stop();
    bool running() const noexcept { return running_.load(std::memory_order_acquire); }
    void setMinLevel(LogLevel l) noexcept { minLevel_.store(l, std::memory_order_relaxed); }

    void log(LogLevel level, const char* category, const std::string& message);
    // `message` is truncated to 111 bytes. `value` is an optional numeric field.
    void logRt(LogLevel level, const char* category, const char* message, double value = 0.0) noexcept;

    std::uint64_t droppedRt() const noexcept { return droppedRt_.load(std::memory_order_relaxed); }

    ~Logger();

private:
    Logger();
    struct RtRecord {
        TimestampNs t;
        double value;
        LogLevel level;
        char category[15];
        char message[112];
    };
    struct Record {
        TimestampNs t;
        LogLevel level;
        std::string category, message;
        bool hasValue;
        double value;
    };
    void run();
    void write(const Record& r);

    SpscQueue<RtRecord> rt_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Record> pending_;
    std::ofstream out_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    std::atomic<LogLevel> minLevel_{LogLevel::Info};
    std::atomic<std::uint64_t> droppedRt_{0};
};

inline void logInfo(const char* cat, const std::string& msg) { Logger::instance().log(LogLevel::Info, cat, msg); }
inline void logWarn(const char* cat, const std::string& msg) { Logger::instance().log(LogLevel::Warn, cat, msg); }
inline void logError(const char* cat, const std::string& msg) { Logger::instance().log(LogLevel::Error, cat, msg); }

}  // namespace sa
