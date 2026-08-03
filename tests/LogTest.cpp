#include <game_core/Log.h>

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

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "cpp_game_core_log_test";
    const auto path = directory / "test.log";
    const auto ignoredPath = directory / "ignored.log";
    std::error_code fileError;
    std::filesystem::remove_all(directory, fileError);
    std::filesystem::create_directories(directory);
    std::ofstream(path) << "previous run entry\n";

    Log::LogConfig config;
    config.level = Log::Level::Debug;
    config.flushIntervalSeconds = 0.05;
    config.rateLimitEntryTtlSeconds = 0.0;
    config.maxRateLimitEntries = 2;
    Log::initialize(path, config);
    Log::initialize(ignoredPath, config);

    assert(Log::isEnabled(Log::Level::Debug));
    assert(!Log::isEnabled(Log::Level::Trace));
    assert(!Log::isRateLimited("RepeatedEvent", 60.0));
    assert(Log::isRateLimited("RepeatedEvent", 60.0));

    std::ostringstream capturedConsole;
    auto *originalCout = std::cout.rdbuf(capturedConsole.rdbuf());
    Log::info("InfoTest", "Informational message");
    Log::debug("DebugTest", "Debug message");
    Log::trace("LogTrace", "This trace entry must be suppressed");
    std::cout << "raw cout entry\n";

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
    std::cout.rdbuf(originalCout);

    Log::warning("WarningTest", "Warning is visible and flushed");
    Log::error("ErrorTest", "Error is visible and flushed");
    for (int i = 0; i < 20; ++i)
        assert(!Log::isRateLimited("dynamic:" + std::to_string(i), 1.0));

    Log::shutdown();
    Log::shutdown();

    const auto contents = readFile(path);
    const std::regex timestamp(R"(\[[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}\] \[INFO\])");
    assert(std::regex_search(contents, timestamp));
    assert(contents.find("previous run entry") != std::string::npos);
    assert(contents.find("===== Log session started:") != std::string::npos);
    assert(contents.find("Visible timer completed in") != std::string::npos);
    assert(contents.find("Hidden timer completed in") == std::string::npos);
    assert(contents.find("This trace entry must be suppressed") == std::string::npos);
    assert(contents.find("raw cout entry") == std::string::npos);
    assert(capturedConsole.str().find("Informational message") != std::string::npos);
    assert(capturedConsole.str().find("raw cout entry") != std::string::npos);
    assert(!std::filesystem::exists(ignoredPath));

    Log::LogConfig errorOnly;
    errorOnly.level = Log::Level::Error;
    Log::initialize(path, errorOnly);
    assert(!Log::isEnabled(Log::Level::Warning));
    assert(Log::isEnabled(Log::Level::Error));
    Log::shutdown();

    const auto combinedContents = readFile(path);
    const std::string divider = "===== Log session started:";
    const auto firstDivider = combinedContents.find(divider);
    assert(firstDivider != std::string::npos);
    assert(combinedContents.find(divider, firstDivider + divider.size()) != std::string::npos);

    std::filesystem::remove_all(directory, fileError);
    return 0;
}
