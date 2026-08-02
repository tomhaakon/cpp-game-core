#include <game_core/Log.h>

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace Log {
namespace {

class TeeStreamBuf : public std::streambuf {
public:
    TeeStreamBuf(std::streambuf *console, std::streambuf *file) : console_(console), file_(file) {}

protected:
    int overflow(int character) override {
        if (character == traits_type::eof()) return traits_type::not_eof(character);
        const char value = static_cast<char>(character);
        const bool consoleOk = console_ && console_->sputc(value) != traits_type::eof();
        const bool fileOk = file_ && file_->sputc(value) != traits_type::eof();
        if (fileOk && value == '\n') file_->pubsync();
        return consoleOk && fileOk ? character : traits_type::eof();
    }

    std::streamsize xsputn(const char *text, std::streamsize count) override {
        const auto consoleWritten = console_ ? console_->sputn(text, count) : 0;
        const auto fileWritten = file_ ? file_->sputn(text, count) : 0;
        if (fileWritten > 0 && std::find(text, text + fileWritten, '\n') != text + fileWritten)
            file_->pubsync();
        return std::min(consoleWritten, fileWritten);
    }

    int sync() override {
        const int consoleResult = console_ ? console_->pubsync() : 0;
        const int fileResult = file_ ? file_->pubsync() : 0;
        return consoleResult == 0 && fileResult == 0 ? 0 : -1;
    }

private:
    std::streambuf *console_;
    std::streambuf *file_;
};

std::ofstream logFile;
std::filesystem::path logPath;
std::mutex logMutex;
std::streambuf *originalCout = nullptr;
std::streambuf *originalCerr = nullptr;
std::streambuf *originalClog = nullptr;
std::unique_ptr<TeeStreamBuf> coutTee;
std::unique_ptr<TeeStreamBuf> cerrTee;
std::unique_ptr<TeeStreamBuf> clogTee;
Level configuredLevel = Level::Info;
int raylibLevel = LOG_WARNING;
bool alwaysShowWarnings = true;
std::unordered_map<std::string, double> configuredRateLimits;
std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastLogTimes;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Level parseLevel(const std::string &value, Level fallback) {
    const auto name = lower(value);
    if (name == "error") return Level::Error;
    if (name == "warning" || name == "warn") return Level::Warning;
    if (name == "info") return Level::Info;
    if (name == "debug") return Level::Debug;
    if (name == "trace") return Level::Trace;
    return fallback;
}

int parseRaylibLevel(const std::string &value, int fallback) {
    const auto name = lower(value);
    if (name == "error") return LOG_ERROR;
    if (name == "warning" || name == "warn") return LOG_WARNING;
    if (name == "info") return LOG_INFO;
    if (name == "debug") return LOG_DEBUG;
    if (name == "trace") return LOG_TRACE;
    return fallback;
}

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

std::tm localTime(std::time_t value) {
    std::tm result{};
#if defined(_WIN32)
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

std::string formattedTime(const char *format) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto local = localTime(time);
    std::ostringstream output;
    output << std::put_time(&local, format);
    return output.str();
}

int nextLogIndex(const std::filesystem::path &directory) {
    int result = 1;
    std::error_code error;
    if (!std::filesystem::exists(directory, error)) return result;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end;
         it.increment(error)) {
        if (!it->is_regular_file(error) || it->path().extension() != ".txt") continue;
        const auto stem = it->path().stem().string();
        const auto separator = stem.find('_');
        if (separator == std::string::npos) continue;
        try { result = std::max(result, std::stoi(stem.substr(0, separator)) + 1); }
        catch (...) {}
    }
    return result;
}

void restoreStreams() {
    if (originalCout) std::cout.rdbuf(originalCout);
    if (originalCerr) std::cerr.rdbuf(originalCerr);
    if (originalClog) std::clog.rdbuf(originalClog);
    originalCout = originalCerr = originalClog = nullptr;
    coutTee.reset();
    cerrTee.reset();
    clogTee.reset();
}

void redirectStreams() {
    originalCout = std::cout.rdbuf();
    originalCerr = std::cerr.rdbuf();
    originalClog = std::clog.rdbuf();
    coutTee = std::make_unique<TeeStreamBuf>(originalCout, logFile.rdbuf());
    cerrTee = std::make_unique<TeeStreamBuf>(originalCerr, logFile.rdbuf());
    clogTee = std::make_unique<TeeStreamBuf>(originalClog, logFile.rdbuf());
    std::cout.rdbuf(coutTee.get());
    std::cerr.rdbuf(cerrTee.get());
    std::clog.rdbuf(clogTee.get());
}

void writeConsole(Level level, const std::string &line) {
    auto *buffer = (level == Level::Error || level == Level::Warning)
                       ? (originalCerr ? originalCerr : std::cerr.rdbuf())
                       : (originalCout ? originalCout : std::cout.rdbuf());
    if (!buffer) return;
    buffer->sputn(line.data(), static_cast<std::streamsize>(line.size()));
    buffer->sputc('\n');
    buffer->pubsync();
}

} // namespace

void configureDefaults() {
    std::lock_guard<std::mutex> lock(logMutex);
    configuredLevel = Level::Info;
    raylibLevel = LOG_WARNING;
    alwaysShowWarnings = true;
    configuredRateLimits.clear();
    lastLogTimes.clear();
}

void configureFromJson(const nlohmann::json &root) {
    if (!root.contains("logging") || !root["logging"].is_object()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    const auto &logging = root["logging"];
    if (logging.contains("level") && logging["level"].is_string())
        configuredLevel = parseLevel(logging["level"].get<std::string>(), configuredLevel);
    if (logging.contains("raylib_level") && logging["raylib_level"].is_string())
        raylibLevel = parseRaylibLevel(logging["raylib_level"].get<std::string>(), raylibLevel);
    if (logging.contains("always_show_warnings") && logging["always_show_warnings"].is_boolean())
        alwaysShowWarnings = logging["always_show_warnings"].get<bool>();
    if (logging.contains("rate_limits") && logging["rate_limits"].is_object()) {
        for (auto it = logging["rate_limits"].begin(); it != logging["rate_limits"].end(); ++it)
            if (it.value().is_number())
                configuredRateLimits[it.key()] = std::max(0.0, it.value().get<double>());
    }
    SetTraceLogLevel(raylibLevel);
}

void applyRaylibTraceLevel() {
    std::lock_guard<std::mutex> lock(logMutex);
    SetTraceLogLevel(raylibLevel);
}

bool isEnabled(Level level) {
    std::lock_guard<std::mutex> lock(logMutex);
    return level == Level::Error || (level == Level::Warning && alwaysShowWarnings) ||
           static_cast<int>(level) <= static_cast<int>(configuredLevel);
}

bool isRateLimited(const std::string &key, double seconds) {
    std::lock_guard<std::mutex> lock(logMutex);
    const auto separator = key.find(':');
    const auto configKey = key.substr(0, separator);
    const auto configured = configuredRateLimits.find(configKey);
    if (configured != configuredRateLimits.end()) seconds = configured->second;
    if (seconds <= 0.0) return false;
    const auto now = std::chrono::steady_clock::now();
    const auto previous = lastLogTimes.find(key);
    if (previous == lastLogTimes.end()) {
        lastLogTimes[key] = now;
        return false;
    }
    if (std::chrono::duration<double>(now - previous->second).count() < seconds) return true;
    previous->second = now;
    return false;
}

void initializeFileLogging(const std::filesystem::path &logsDirectory,
                           const std::string &applicationName) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) return;
    try {
        std::filesystem::create_directories(logsDirectory);
        logPath = logsDirectory / (std::to_string(nextLogIndex(logsDirectory)) + "_" +
                                   formattedTime("%d-%m-%Y_%H-%M-%S") + ".txt");
        logFile.open(logPath, std::ios::out | std::ios::app);
        if (!logFile) throw std::runtime_error("could not open " + logPath.string());
        redirectStreams();
        logFile << "===== " << applicationName << " log started: "
                << formattedTime("%d-%m-%Y %H:%M:%S") << " =====\n";
        logFile.flush();
        writeConsole(Level::Info, "[Log] File logging enabled: " + logPath.string());
    } catch (const std::exception &exception) {
        restoreStreams();
        if (logFile.is_open()) logFile.close();
        logFile.clear();
        logPath.clear();
        std::cerr << "[Log] Warning: failed to initialize file logging: " << exception.what() << '\n';
    }
}

void shutdownFileLogging() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile << "===== Log ended =====\n";
        logFile.flush();
    }
    restoreStreams();
    if (logFile.is_open()) logFile.close();
    logFile.clear();
    logPath.clear();
}

bool isFileLoggingEnabled() {
    std::lock_guard<std::mutex> lock(logMutex);
    return logFile.is_open();
}

std::filesystem::path currentLogFilePath() {
    std::lock_guard<std::mutex> lock(logMutex);
    return logPath;
}

void message(Level level, const std::string &tag, const std::string &text) {
    if (!isEnabled(level)) return;
    const std::string line = "[" + tag + "] " + text;
    std::lock_guard<std::mutex> lock(logMutex);
    writeConsole(level, line);
    if (logFile.is_open()) {
        logFile << '[' << levelName(level) << "] " << line << '\n';
        logFile.flush();
    }
}

void info(const std::string &tag, const std::string &text) { message(Level::Info, tag, text); }
void warning(const std::string &tag, const std::string &text) { message(Level::Warning, tag, text); }
void error(const std::string &tag, const std::string &text) { message(Level::Error, tag, text); }
void debug(const std::string &tag, const std::string &text) { message(Level::Debug, tag, text); }
void trace(const std::string &tag, const std::string &text) { message(Level::Trace, tag, text); }

} // namespace Log
