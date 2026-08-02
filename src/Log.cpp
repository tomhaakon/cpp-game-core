#include <game_core/Log.h>

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

namespace Log {
namespace {

constexpr Level defaultLevel = Level::Info;
constexpr int defaultRaylibLevel = LOG_WARNING;
constexpr bool defaultAlwaysShowWarnings = true;
constexpr double defaultFlushIntervalSeconds = 1.0;
constexpr std::size_t defaultMaxLogFiles = 50;
constexpr double defaultRateLimitEntryTtlSeconds = 600.0;
constexpr std::size_t defaultMaxRateLimitEntries = 2048;

struct RateLimitEntry {
    std::chrono::steady_clock::time_point lastEmitted;
    std::chrono::steady_clock::time_point lastAccessed;
};

void flushIfDueLocked();

class TeeStreamBuf : public std::streambuf {
public:
    TeeStreamBuf(std::streambuf *console, std::streambuf *file, std::mutex &mutex)
        : console_(console), file_(file), mutex_(mutex) {}

protected:
    int overflow(int character) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (character == traits_type::eof()) return traits_type::not_eof(character);
        const char value = static_cast<char>(character);
        const bool consoleOk = console_ && console_->sputc(value) != traits_type::eof();
        const bool fileOk = file_ && file_->sputc(value) != traits_type::eof();
        if (value == '\n') flushIfDueLocked();
        return consoleOk && fileOk ? character : traits_type::eof();
    }

    std::streamsize xsputn(const char *text, std::streamsize count) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto consoleWritten = console_ ? console_->sputn(text, count) : 0;
        const auto fileWritten = file_ ? file_->sputn(text, count) : 0;
        if (fileWritten > 0 && std::find(text, text + fileWritten, '\n') != text + fileWritten)
            flushIfDueLocked();
        return std::min(consoleWritten, fileWritten);
    }

    int sync() override {
        std::lock_guard<std::mutex> lock(mutex_);
        const int consoleResult = console_ ? console_->pubsync() : 0;
        const int fileResult = file_ ? file_->pubsync() : 0;
        return consoleResult == 0 && fileResult == 0 ? 0 : -1;
    }

private:
    std::streambuf *console_;
    std::streambuf *file_;
    std::mutex &mutex_;
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
Level configuredLevel = defaultLevel;
int raylibLevel = defaultRaylibLevel;
bool alwaysShowWarnings = defaultAlwaysShowWarnings;
double flushIntervalSeconds = defaultFlushIntervalSeconds;
std::size_t maxLogFiles = defaultMaxLogFiles;
double rateLimitEntryTtlSeconds = defaultRateLimitEntryTtlSeconds;
std::size_t maxRateLimitEntries = defaultMaxRateLimitEntries;
std::unordered_map<std::string, double> configuredRateLimits;
std::unordered_map<std::string, RateLimitEntry> lastLogTimes;
std::chrono::steady_clock::time_point lastFileFlush = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point lastRateLimitCleanup = std::chrono::steady_clock::now();
std::once_flag shutdownRegistration;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool parseLevel(const std::string &value, Level &result) {
    const auto name = lower(value);
    if (name == "error") result = Level::Error;
    else if (name == "warning" || name == "warn") result = Level::Warning;
    else if (name == "info") result = Level::Info;
    else if (name == "debug") result = Level::Debug;
    else if (name == "trace") result = Level::Trace;
    else return false;
    return true;
}

bool parseRaylibLevel(const std::string &value, int &result) {
    const auto name = lower(value);
    if (name == "error") result = LOG_ERROR;
    else if (name == "warning" || name == "warn") result = LOG_WARNING;
    else if (name == "info") result = LOG_INFO;
    else if (name == "debug") result = LOG_DEBUG;
    else if (name == "trace") result = LOG_TRACE;
    else return false;
    return true;
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

bool levelEnabledLocked(Level level) {
    if (level == Level::Error) return true;
    if (level == Level::Warning && alwaysShowWarnings) return true;
    switch (configuredLevel) {
    case Level::Error: return level == Level::Error;
    case Level::Warning: return level == Level::Error || level == Level::Warning;
    case Level::Info: return level == Level::Error || level == Level::Warning || level == Level::Info;
    case Level::Debug: return level != Level::Trace;
    case Level::Trace: return true;
    }
    return false;
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

std::string timestampWithMilliseconds() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) % 1000;
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto local = localTime(time);
    std::ostringstream output;
    output << std::put_time(&local, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
           << milliseconds.count();
    return output.str();
}

int processId() {
#if defined(_WIN32)
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

bool isGeneratedLogFile(const std::filesystem::path &path) {
    static const std::regex pattern(
        R"(^[0-9]+_[0-9]{2}-[0-9]{2}-[0-9]{4}_[0-9]{2}-[0-9]{2}-[0-9]{2}_pid-[0-9]+\.txt$)");
    return std::regex_match(path.filename().string(), pattern);
}

int nextLogIndex(const std::filesystem::path &directory) {
    int result = 1;
    std::error_code error;
    if (!std::filesystem::exists(directory, error)) return result;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end;
         it.increment(error)) {
        if (!it->is_regular_file(error) || !isGeneratedLogFile(it->path())) continue;
        const auto stem = it->path().stem().string();
        try { result = std::max(result, std::stoi(stem.substr(0, stem.find('_'))) + 1); }
        catch (...) {}
    }
    return result;
}

std::vector<std::string> enforceRetention(const std::filesystem::path &directory) {
    struct FileInfo {
        std::filesystem::path path;
        std::filesystem::file_time_type modified;
    };
    std::vector<std::string> warnings;
    std::vector<FileInfo> files;
    std::error_code error;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end;
         it.increment(error)) {
        if (!it->is_regular_file(error) || !isGeneratedLogFile(it->path())) continue;
        const auto modified = it->last_write_time(error);
        if (!error) files.push_back({it->path(), modified});
        else {
            warnings.push_back("Could not inspect old log '" + it->path().string() + "': " +
                               error.message());
            error.clear();
        }
    }
    if (error) warnings.push_back("Could not scan log directory: " + error.message());
    std::sort(files.begin(), files.end(), [](const FileInfo &left, const FileInfo &right) {
        return left.modified < right.modified;
    });
    const std::size_t filesToKeep = maxLogFiles > 0 ? maxLogFiles - 1 : 0;
    const std::size_t removeCount = files.size() > filesToKeep ? files.size() - filesToKeep : 0;
    for (std::size_t i = 0; i < removeCount; ++i) {
        error.clear();
        if (!std::filesystem::remove(files[i].path, error) || error)
            warnings.push_back("Could not remove old log '" + files[i].path.string() + "': " +
                               (error ? error.message() : "unknown error"));
    }
    return warnings;
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
    coutTee = std::make_unique<TeeStreamBuf>(originalCout, logFile.rdbuf(), logMutex);
    cerrTee = std::make_unique<TeeStreamBuf>(originalCerr, logFile.rdbuf(), logMutex);
    clogTee = std::make_unique<TeeStreamBuf>(originalClog, logFile.rdbuf(), logMutex);
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

void flushIfDueLocked() {
    if (!logFile.is_open() || flushIntervalSeconds <= 0.0) return;
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - lastFileFlush).count() < flushIntervalSeconds) return;
    logFile.flush();
    lastFileFlush = now;
}

void cleanupRateLimitsLocked(std::chrono::steady_clock::time_point now, bool forceForCapacity) {
    const bool cleanupDue = std::chrono::duration<double>(now - lastRateLimitCleanup).count() >= 60.0;
    if (!cleanupDue && !forceForCapacity) return;
    const auto ttl = std::chrono::duration<double>(rateLimitEntryTtlSeconds);
    for (auto it = lastLogTimes.begin(); it != lastLogTimes.end();) {
        if (rateLimitEntryTtlSeconds == 0.0 || now - it->second.lastAccessed >= ttl)
            it = lastLogTimes.erase(it);
        else
            ++it;
    }
    while (lastLogTimes.size() >= maxRateLimitEntries && !lastLogTimes.empty()) {
        const auto oldest = std::min_element(lastLogTimes.begin(), lastLogTimes.end(),
            [](const auto &left, const auto &right) {
                return left.second.lastAccessed < right.second.lastAccessed;
            });
        lastLogTimes.erase(oldest);
    }
    lastRateLimitCleanup = now;
}

Level raylibToLevel(int level) {
    switch (level) {
    case LOG_FATAL:
    case LOG_ERROR: return Level::Error;
    case LOG_WARNING: return Level::Warning;
    case LOG_INFO: return Level::Info;
    case LOG_DEBUG: return Level::Debug;
    case LOG_TRACE: return Level::Trace;
    default: return Level::Info;
    }
}

void raylibTraceCallback(int level, const char *format, va_list arguments) {
    try {
        va_list copy;
        va_copy(copy, arguments);
        const int required = std::vsnprintf(nullptr, 0, format, copy);
        va_end(copy);
        if (required < 0) return;
        std::vector<char> buffer(static_cast<std::size_t>(required) + 1);
        va_copy(copy, arguments);
        std::vsnprintf(buffer.data(), buffer.size(), format, copy);
        va_end(copy);
        message(raylibToLevel(level), "Raylib", std::string(buffer.data(), static_cast<std::size_t>(required)));
    } catch (...) {
        // Exceptions must never cross Raylib's C callback boundary.
    }
}

void applyRaylibConfiguration(int level) {
    SetTraceLogCallback(raylibTraceCallback);
    SetTraceLogLevel(level);
}

} // namespace

void configureDefaults() {
    {
        std::lock_guard<std::mutex> lock(logMutex);
        configuredLevel = defaultLevel;
        raylibLevel = defaultRaylibLevel;
        alwaysShowWarnings = defaultAlwaysShowWarnings;
        flushIntervalSeconds = defaultFlushIntervalSeconds;
        maxLogFiles = defaultMaxLogFiles;
        rateLimitEntryTtlSeconds = defaultRateLimitEntryTtlSeconds;
        maxRateLimitEntries = defaultMaxRateLimitEntries;
        configuredRateLimits.clear();
        lastLogTimes.clear();
    }
    applyRaylibConfiguration(defaultRaylibLevel);
}

void configureFromJson(const nlohmann::json &root) {
    if (!root.contains("logging") || !root["logging"].is_object()) return;
    std::vector<std::string> warnings;
    int appliedRaylibLevel = defaultRaylibLevel;
    {
        std::lock_guard<std::mutex> lock(logMutex);
        configuredLevel = defaultLevel;
        raylibLevel = defaultRaylibLevel;
        alwaysShowWarnings = defaultAlwaysShowWarnings;
        flushIntervalSeconds = defaultFlushIntervalSeconds;
        maxLogFiles = defaultMaxLogFiles;
        rateLimitEntryTtlSeconds = defaultRateLimitEntryTtlSeconds;
        maxRateLimitEntries = defaultMaxRateLimitEntries;
        configuredRateLimits.clear();
        const auto &logging = root["logging"];
        try {
            if (logging.contains("level")) {
                if (!logging["level"].is_string() ||
                    !parseLevel(logging["level"].get<std::string>(), configuredLevel))
                    warnings.push_back("Invalid logging.level; using info");
            }
            if (logging.contains("raylib_level")) {
                if (!logging["raylib_level"].is_string() ||
                    !parseRaylibLevel(logging["raylib_level"].get<std::string>(), raylibLevel))
                    warnings.push_back("Invalid logging.raylib_level; using warning");
            }
            if (logging.contains("always_show_warnings")) {
                if (logging["always_show_warnings"].is_boolean())
                    alwaysShowWarnings = logging["always_show_warnings"].get<bool>();
                else warnings.push_back("Invalid logging.always_show_warnings; using true");
            }
            auto readNonNegative = [&](const char *name, double &target) {
                if (!logging.contains(name)) return;
                if (logging[name].is_number()) {
                    const double value = logging[name].get<double>();
                    if (std::isfinite(value) && value >= 0.0) { target = value; return; }
                }
                warnings.push_back(std::string("Invalid logging.") + name + "; using default");
            };
            readNonNegative("flush_interval_seconds", flushIntervalSeconds);
            readNonNegative("rate_limit_entry_ttl_seconds", rateLimitEntryTtlSeconds);
            auto readPositiveSize = [&](const char *name, std::size_t &target) {
                if (!logging.contains(name)) return;
                if (logging[name].is_number_unsigned()) {
                    const auto value = logging[name].get<std::size_t>();
                    if (value > 0) { target = value; return; }
                } else if (logging[name].is_number_integer()) {
                    const auto value = logging[name].get<long long>();
                    if (value > 0) { target = static_cast<std::size_t>(value); return; }
                }
                warnings.push_back(std::string("Invalid logging.") + name + "; using default");
            };
            readPositiveSize("max_log_files", maxLogFiles);
            readPositiveSize("max_rate_limit_entries", maxRateLimitEntries);
            if (logging.contains("rate_limits")) {
                if (logging["rate_limits"].is_object()) {
                    for (auto it = logging["rate_limits"].begin(); it != logging["rate_limits"].end(); ++it) {
                        if (!it.value().is_number()) {
                            warnings.push_back("Invalid rate limit for '" + it.key() + "'; ignoring it");
                            continue;
                        }
                        const double value = it.value().get<double>();
                        if (!std::isfinite(value) || value < 0.0)
                            warnings.push_back("Invalid rate limit for '" + it.key() + "'; ignoring it");
                        else configuredRateLimits[it.key()] = value;
                    }
                } else warnings.push_back("Invalid logging.rate_limits; using an empty map");
            }
        } catch (const std::exception &exception) {
            warnings.push_back(std::string("Malformed logging configuration: ") + exception.what());
        }
        appliedRaylibLevel = raylibLevel;
    }
    applyRaylibConfiguration(appliedRaylibLevel);
    for (const auto &warningText : warnings) warning("LogConfig", warningText);
}

void applyRaylibTraceLevel() {
    int level;
    {
        std::lock_guard<std::mutex> lock(logMutex);
        level = raylibLevel;
    }
    applyRaylibConfiguration(level);
}

bool isEnabled(Level level) {
    std::lock_guard<std::mutex> lock(logMutex);
    return levelEnabledLocked(level);
}

bool isRateLimited(const std::string &key, double seconds) {
    std::lock_guard<std::mutex> lock(logMutex);
    const auto separator = key.find(':');
    const auto configKey = key.substr(0, separator);
    const auto configured = configuredRateLimits.find(configKey);
    if (configured != configuredRateLimits.end()) seconds = configured->second;
    if (!std::isfinite(seconds) || seconds <= 0.0) return false;
    const auto now = std::chrono::steady_clock::now();
    cleanupRateLimitsLocked(now, lastLogTimes.size() >= maxRateLimitEntries);
    const auto previous = lastLogTimes.find(key);
    if (previous == lastLogTimes.end()) {
        lastLogTimes.emplace(key, RateLimitEntry{now, now});
        return false;
    }
    previous->second.lastAccessed = now;
    if (std::chrono::duration<double>(now - previous->second.lastEmitted).count() < seconds)
        return true;
    previous->second.lastEmitted = now;
    return false;
}

void initializeFileLogging(const std::filesystem::path &logsDirectory,
                           const std::string &applicationName) {
    std::vector<std::string> cleanupWarnings;
    std::string failure;
    {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open()) return;
        try {
            std::filesystem::create_directories(logsDirectory);
            cleanupWarnings = enforceRetention(logsDirectory);
            logPath = logsDirectory / (std::to_string(nextLogIndex(logsDirectory)) + "_" +
                                       formattedTime("%d-%m-%Y_%H-%M-%S") + "_pid-" +
                                       std::to_string(processId()) + ".txt");
            logFile.open(logPath, std::ios::out | std::ios::trunc);
            if (!logFile) throw std::runtime_error("could not open " + logPath.string());
            redirectStreams();
            std::call_once(shutdownRegistration, [] { std::atexit(shutdownFileLogging); });
            logFile << "===== " << applicationName << " log started: "
                    << formattedTime("%d-%m-%Y %H:%M:%S") << " =====\n";
            logFile.flush();
            lastFileFlush = std::chrono::steady_clock::now();
            writeConsole(Level::Info, "[Log] File logging enabled: " + logPath.string());
        } catch (const std::exception &exception) {
            restoreStreams();
            if (logFile.is_open()) logFile.close();
            logFile.clear();
            logPath.clear();
            failure = exception.what();
        }
    }
    if (!failure.empty()) {
        warning("Log", "Failed to initialize file logging: " + failure);
        return;
    }
    for (const auto &warningText : cleanupWarnings) warning("LogRetention", warningText);
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
    return logFile.is_open() && logFile.good();
}

std::filesystem::path currentLogFilePath() {
    std::lock_guard<std::mutex> lock(logMutex);
    return logPath;
}

void message(Level level, const std::string &tag, const std::string &text) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (!levelEnabledLocked(level)) return;
    const std::string line = "[" + timestampWithMilliseconds() + "] [" + levelName(level) +
                             "] [" + tag + "] " + text;
    writeConsole(level, line);
    if (logFile.is_open()) {
        logFile << line << '\n';
        if (level == Level::Error || level == Level::Warning) {
            logFile.flush();
            lastFileFlush = std::chrono::steady_clock::now();
        } else {
            flushIfDueLocked();
        }
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
        text << operation_ << " completed in " << std::fixed << std::setprecision(2) << elapsed << " ms";
        message(level_, tag_, text.str());
    } catch (...) {
        // Destructors must not throw, including during stack unwinding.
    }
}

} // namespace Log
