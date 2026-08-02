#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace Log {

enum class Level { Error, Warning, Info, Debug, Trace };

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
