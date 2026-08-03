#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>

namespace Log {

enum class Level : int { Error = 0, Warning = 1, Info = 2, Debug = 3, Trace = 4 };

struct LogConfig {
    Level level = Level::Info;
    double flushIntervalSeconds = 1.0;
    bool flushWarnings = true;
    double rateLimitEntryTtlSeconds = 600.0;
    std::size_t maxRateLimitEntries = 2048;
};

[[nodiscard]] bool initialize(const std::filesystem::path &filePath,
                              const LogConfig &config = {});
[[nodiscard]] bool deleteOldLogs(const std::filesystem::path &directory,
                                 std::size_t maxFiles);
void flush();
void shutdown();

[[nodiscard]] bool isEnabled(Level level = Level::Info);
[[nodiscard]] bool isRateLimited(const std::string &key, double seconds);

void message(Level level, const std::string &tag, const std::string &text);
void info(const std::string &tag, const std::string &text);
void warning(const std::string &tag, const std::string &text);
void error(const std::string &tag, const std::string &text);
void debug(const std::string &tag, const std::string &text);
void trace(const std::string &tag, const std::string &text);

class ScopedTimer {
public:
    ScopedTimer(Level level, std::string tag, std::string operation,
                double minimumMilliseconds = 0.0);
    ~ScopedTimer() noexcept;

    ScopedTimer(const ScopedTimer &) = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;
    ScopedTimer(ScopedTimer &&) = delete;
    ScopedTimer &operator=(ScopedTimer &&) = delete;

private:
    Level level_;
    std::string tag_;
    std::string operation_;
    double minimumMilliseconds_;
    bool enabled_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace Log
