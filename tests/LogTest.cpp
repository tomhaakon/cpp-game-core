#include <game_core/Log.h>

#include <raylib.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string readFile(const std::filesystem::path &path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::size_t generatedLogCount(const std::filesystem::path &directory) {
    const std::regex pattern(
        R"(^[0-9]+_[0-9]{2}-[0-9]{2}-[0-9]{4}_[0-9]{2}-[0-9]{2}-[0-9]{2}_pid-[0-9]+\.txt$)");
    std::size_t count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file() && std::regex_match(entry.path().filename().string(), pattern))
            ++count;
    return count;
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "cpp_game_core_log_test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory);

    for (int i = 1; i <= 3; ++i) {
        std::ofstream oldLog(directory / (std::to_string(i) + "_01-01-2020_00-00-0" +
                                          std::to_string(i) + "_pid-1.txt"));
        oldLog << "old\n";
    }
    std::ofstream(directory / "unrelated.txt") << "keep me\n";

    nlohmann::json configuration = {
        {"logging", {
            {"level", "debug"},
            {"raylib_level", "trace"},
            {"always_show_warnings", true},
            {"flush_interval_seconds", 0.05},
            {"max_log_files", 2},
            {"rate_limit_entry_ttl_seconds", 0.0},
            {"max_rate_limit_entries", 2},
            {"rate_limits", {{"FishActivity", 60.0}}}
        }}
    };
    Log::configureFromJson(configuration);
    assert(Log::isEnabled(Log::Level::Debug));
    assert(!Log::isEnabled(Log::Level::Trace));
    assert(!Log::isRateLimited("FishActivity:non_water", 0.0));
    assert(Log::isRateLimited("FishActivity:non_water", 0.0));

    Log::initializeFileLogging(directory, "Log development test");
    const auto path = Log::currentLogFilePath();
    assert(Log::isFileLoggingEnabled());
    Log::initializeFileLogging(directory, "Ignored duplicate initialization");
    assert(Log::currentLogFilePath() == path);

    Log::info("MapLoader", "Loaded mountain.tmx");
    Log::debug("FishActivity", "Rejected activity outside water");
    Log::trace("Filter", "This trace entry must be suppressed");
    Log::warning("WarningTest", "Warning is visible and flushed");
    Log::error("ErrorTest", "Error is visible and flushed");
    std::cout << "raw cout entry\n";
    TraceLog(LOG_WARNING, "Raylib callback test %d", 42);

    {
        Log::ScopedTimer timer(Log::Level::Debug, "Timer", "Visible timer", 0.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        Log::ScopedTimer timer(Log::Level::Debug, "Timer", "Hidden timer", 1000.0);
    }

    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) {
        threads.emplace_back([thread] {
            for (int entry = 0; entry < 25; ++entry)
                Log::info("Concurrent", "thread=" + std::to_string(thread) +
                                             " entry=" + std::to_string(entry));
        });
    }
    for (auto &thread : threads) thread.join();

    for (int i = 0; i < 20; ++i)
        assert(!Log::isRateLimited("dynamic:" + std::to_string(i), 1.0));

    Log::shutdownFileLogging();
    Log::shutdownFileLogging();
    assert(!Log::isFileLoggingEnabled());

    const auto contents = readFile(path);
    const std::regex timestamp(R"(\[[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}\] \[INFO\])");
    assert(std::regex_search(contents, timestamp));
    assert(contents.find("[Raylib] Raylib callback test 42") != std::string::npos);
    assert(contents.find("Visible timer completed in") != std::string::npos);
    assert(contents.find("Hidden timer completed in") == std::string::npos);
    assert(contents.find("This trace entry must be suppressed") == std::string::npos);
    assert(contents.find("raw cout entry") != std::string::npos);
    assert(generatedLogCount(directory) == 2);
    assert(std::filesystem::exists(directory / "unrelated.txt"));

    configuration["logging"]["rate_limits"] = nlohmann::json::object();
    configuration["logging"]["level"] = "error";
    configuration["logging"]["always_show_warnings"] = false;
    Log::configureFromJson(configuration);
    assert(!Log::isEnabled(Log::Level::Warning));
    assert(!Log::isRateLimited("FishActivity:non_water", 0.0));

    std::filesystem::remove_all(directory, error);
    return 0;
}
