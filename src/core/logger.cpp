#include "stickyaim/core/logger.hpp"

#include <chrono>
#include <cstring>

#include <nlohmann/json.hpp>

#include "stickyaim/core/clock.hpp"

namespace sa {
namespace {

const char* levelName(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warn: return "warn";
        case LogLevel::Error: return "error";
    }
    return "info";
}

}  // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() : rt_(4096) {}

Logger::~Logger() { stop(); }

bool Logger::start(const std::string& path, LogLevel minLevel) {
    stop();
    out_.open(path, std::ios::out | std::ios::app);
    if (!out_) return false;
    minLevel_.store(minLevel);
    stop_.store(false);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { run(); });
    return true;
}

void Logger::stop() {
    if (!running_.exchange(false)) return;
    {
        std::lock_guard lk(mu_);
        stop_.store(true);
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    out_.close();
}

void Logger::log(LogLevel level, const char* category, const std::string& message) {
    if (!running() || level < minLevel_.load(std::memory_order_relaxed)) return;
    {
        std::lock_guard lk(mu_);
        if (pending_.size() > 10000) return;  // shed load rather than grow without bound
        pending_.push_back(Record{nowNs(), level, category, message, false, 0.0});
    }
    cv_.notify_one();
}

void Logger::logRt(LogLevel level, const char* category, const char* message, double value) noexcept {
    if (!running() || level < minLevel_.load(std::memory_order_relaxed)) return;
    RtRecord r{};
    r.t = nowNs();
    r.value = value;
    r.level = level;
    std::strncpy(r.category, category, sizeof(r.category) - 1);
    std::strncpy(r.message, message, sizeof(r.message) - 1);
    if (!rt_.push(r)) droppedRt_.fetch_add(1, std::memory_order_relaxed);
    // No notify: the writer polls the RT ring on a short timeout so the input
    // thread never touches the condition variable's mutex.
}

void Logger::write(const Record& r) {
    nlohmann::json j;
    j["t_ns"] = r.t;
    j["level"] = levelName(r.level);
    j["cat"] = r.category;
    j["msg"] = r.message;
    if (r.hasValue) j["value"] = r.value;
    out_ << j.dump() << '\n';
}

void Logger::run() {
    std::deque<Record> batch;
    for (;;) {
        {
            std::unique_lock lk(mu_);
            cv_.wait_for(lk, std::chrono::milliseconds(50), [this] { return stop_.load() || !pending_.empty(); });
            batch.swap(pending_);
        }
        RtRecord rr;
        while (rt_.pop(rr)) write(Record{rr.t, rr.level, rr.category, rr.message, true, rr.value});
        for (const auto& r : batch) write(r);
        batch.clear();
        out_.flush();
        if (stop_.load()) {
            while (rt_.pop(rr)) write(Record{rr.t, rr.level, rr.category, rr.message, true, rr.value});
            std::lock_guard lk(mu_);
            for (const auto& r : pending_) write(r);
            pending_.clear();
            out_.flush();
            return;
        }
    }
}

}  // namespace sa
