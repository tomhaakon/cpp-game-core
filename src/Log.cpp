#include <game_core/Log.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace Log {
namespace {

struct RateLimitEntry {
    std::chrono::steady_clock::time_point lastEmitted;
    std::chrono::steady_clock::time_point lastAccessed;
};

std::mutex logMutex;
std::ofstream logFile;
LogConfig activeConfig;
std::unordered_map<std::string, RateLimitEntry> rateLimitEntries;
std::chrono::steady_clock::time_point lastFileFlush = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point lastRateLimitCleanup = std::chrono::steady_clock::now();
std::once_flag shutdownRegistration;

const char *levelName(Level level) {
    switch (level) {
    case Level::Error: return "ERROR";
    case Level::Warning: return "WARNING";
    case Level::Info: return "INFO";
    case Level::Debug: return "DEBUG";
    case Level::Trace: return "TRACE";
    }
    return "INFO";
}

bool levelEnabled(Level level) {
    return static_cast<int>(level) <= static_cast<int>(activeConfig.level);
}

std::tm localTime(std::time_t value) {
    std::tm result{};
#if defined(_WIN32)
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) % 1000;
    const auto local = localTime(std::chrono::system_clock::to_time_t(now));
    std::ostringstream output;
    output << std::put_time(&local, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
           << milliseconds.count();
    return output.str();
}

std::string sessionTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto local = localTime(std::chrono::system_clock::to_time_t(now));
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

void flushIfDue(std::chrono::steady_clock::time_point now) {
    if (!logFile.is_open() || activeConfig.flushIntervalSeconds <= 0.0) return;
    if (std::chrono::duration<double>(now - lastFileFlush).count() <
        activeConfig.flushIntervalSeconds)
        return;
    logFile.flush();
    lastFileFlush = now;
}

void cleanupRateLimits(std::chrono::steady_clock::time_point now, bool atCapacity) {
    const bool cleanupDue =
        std::chrono::duration<double>(now - lastRateLimitCleanup).count() >= 60.0;
    if (!cleanupDue && !atCapacity) return;

    const auto ttl = std::chrono::duration<double>(activeConfig.rateLimitEntryTtlSeconds);
    for (auto it = rateLimitEntries.begin(); it != rateLimitEntries.end();) {
        if (activeConfig.rateLimitEntryTtlSeconds == 0.0 ||
            now - it->second.lastAccessed >= ttl)
            it = rateLimitEntries.erase(it);
        else
            ++it;
    }

    while (rateLimitEntries.size() >= activeConfig.maxRateLimitEntries &&
           !rateLimitEntries.empty()) {
        const auto oldest = std::min_element(
            rateLimitEntries.begin(), rateLimitEntries.end(),
            [](const auto &left, const auto &right) {
                return left.second.lastAccessed < right.second.lastAccessed;
            });
        rateLimitEntries.erase(oldest);
    }
    lastRateLimitCleanup = now;
}

LogConfig sanitized(LogConfig config) {
    if (!std::isfinite(config.flushIntervalSeconds) || config.flushIntervalSeconds < 0.0)
        config.flushIntervalSeconds = 1.0;
    if (!std::isfinite(config.rateLimitEntryTtlSeconds) ||
        config.rateLimitEntryTtlSeconds < 0.0)
        config.rateLimitEntryTtlSeconds = 600.0;
    if (config.maxRateLimitEntries == 0) config.maxRateLimitEntries = 2048;
    return config;
}

} // namespace

void initialize(const std::filesystem::path &filePath, const LogConfig &config) {
    std::string failure;
    {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open()) return;

        activeConfig = sanitized(config);
        rateLimitEntries.clear();
        try {
            const auto parent = filePath.parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            logFile.open(filePath, std::ios::out | std::ios::app);
            if (!logFile) throw std::runtime_error("could not open " + filePath.string());
            logFile << "\n===== Log session started: " << sessionTimestamp() << " =====\n";
            logFile.flush();
            lastFileFlush = std::chrono::steady_clock::now();
            std::call_once(shutdownRegistration, [] { std::atexit(shutdown); });
        } catch (const std::exception &exception) {
            if (logFile.is_open()) logFile.close();
            logFile.clear();
            failure = exception.what();
        }
    }
    if (!failure.empty()) error("Log", "Failed to initialize file logging: " + failure);
}

void shutdown() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
    logFile.clear();
    rateLimitEntries.clear();
}

bool isEnabled(Level level) {
    std::lock_guard<std::mutex> lock(logMutex);
    return levelEnabled(level);
}

bool isRateLimited(const std::string &key, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) return false;

    std::lock_guard<std::mutex> lock(logMutex);
    const auto now = std::chrono::steady_clock::now();
    cleanupRateLimits(now, rateLimitEntries.size() >= activeConfig.maxRateLimitEntries);
    const auto previous = rateLimitEntries.find(key);
    if (previous == rateLimitEntries.end()) {
        rateLimitEntries.emplace(key, RateLimitEntry{now, now});
        return false;
    }

    previous->second.lastAccessed = now;
    if (std::chrono::duration<double>(now - previous->second.lastEmitted).count() < seconds)
        return true;
    previous->second.lastEmitted = now;
    return false;
}

void message(Level level, const std::string &tag, const std::string &text) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (!levelEnabled(level)) return;

    const std::string line = "[" + timestamp() + "] [" + levelName(level) + "] [" + tag +
                             "] " + text;
    std::ostream &console = level == Level::Error || level == Level::Warning
                                ? static_cast<std::ostream &>(std::cerr)
                                : static_cast<std::ostream &>(std::cout);
    console << line << '\n';

    if (!logFile.is_open()) return;
    logFile << line << '\n';
    const auto now = std::chrono::steady_clock::now();
    if (level == Level::Error || (level == Level::Warning && activeConfig.flushWarnings)) {
        logFile.flush();
        lastFileFlush = now;
    } else {
        flushIfDue(now);
    }
}

void info(const std::string &tag, const std::string &text) { message(Level::Info, tag, text); }
void warning(const std::string &tag, const std::string &text) { message(Level::Warning, tag, text); }
void error(const std::string &tag, const std::string &text) { message(Level::Error, tag, text); }
void debug(const std::string &tag, const std::string &text) { message(Level::Debug, tag, text); }
void trace(const std::string &tag, const std::string &text) { message(Level::Trace, tag, text); }

ScopedTimer::ScopedTimer(Level level, std::string tag, std::string operation,
                         double minimumMilliseconds)
    : level_(level), tag_(std::move(tag)), operation_(std::move(operation)),
      minimumMilliseconds_(std::max(0.0, minimumMilliseconds)), enabled_(isEnabled(level)),
      start_(enabled_ ? std::chrono::steady_clock::now()
                      : std::chrono::steady_clock::time_point{}) {}

ScopedTimer::~ScopedTimer() noexcept {
    if (!enabled_) return;
    try {
        const double elapsed = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - start_).count();
        if (elapsed < minimumMilliseconds_) return;
        std::ostringstream text;
        text << operation_ << " completed in " << std::fixed << std::setprecision(2) << elapsed
             << " ms";
        message(level_, tag_, text.str());
    } catch (...) {
        // Destructors must not throw during stack unwinding.
    }
}

} // namespace Log
