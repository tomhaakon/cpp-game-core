#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace Log {

enum class Level : int { Error = 0, Warning = 1, Info = 2, Debug = 3, Trace = 4 };

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

void configureDefaults();
void configureFromJson(const nlohmann::json &root);
void applyRaylibTraceLevel();

[[nodiscard]] bool isEnabled(Level level = Level::Info);
[[nodiscard]] bool isRateLimited(const std::string &key, double seconds);

void initializeFileLogging(const std::filesystem::path &logsDirectory = "logs",
                           const std::string &applicationName = "Game");
void shutdownFileLogging();
[[nodiscard]] bool isFileLoggingEnabled();
[[nodiscard]] std::filesystem::path currentLogFilePath();

void message(Level level, const std::string &tag, const std::string &text);
void info(const std::string &tag, const std::string &text);
void warning(const std::string &tag, const std::string &text);
void error(const std::string &tag, const std::string &text);
void debug(const std::string &tag, const std::string &text);
void trace(const std::string &tag, const std::string &text);

} // namespace Log
