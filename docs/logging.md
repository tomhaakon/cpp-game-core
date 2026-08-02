# Logging

The logging API provides structured, thread-safe messages without an external logging library.
Prefer `Log::*` to raw `std::cout`: structured entries have timestamps, levels and tags, remain
atomic across threads, participate in filtering, and capture consistently in the log file.

## Setup and files

```cpp
Log::configureDefaults();
Log::configureFromJson(configuration); // optional
Log::initializeFileLogging("logs", "My Game");
```

Logs are stored as `logs/<index>_<date>_<time>_pid-<process-id>.txt`. Shutdown flushes and closes
the file. `shutdownFileLogging()` is safe to call repeatedly and is also registered automatically
at process exit. An orderly shutdown is still recommended before worker threads are destroyed.

The logger retains 50 generated files by default. Retention uses modification times, ignores
unrelated files, and reports cleanup failures without stopping startup.

## Levels and structured messages

Levels are `error`, `warning`, `info`, `debug`, and `trace`. Selecting a level includes it and all
more severe levels. Errors are always enabled. Warnings are also always enabled by default; set
`always_show_warnings` to `false` if the configured threshold should control them.

```cpp
Log::info("MapLoader", "Loaded mountain.tmx");
Log::warning("MapLoader", "Missing spawn layer");
```

Structured output looks like:

```text
[20:51:14.382] [INFO] [MapLoader] Loaded mountain.tmx
```

Errors and warnings flush immediately. Less severe messages remain buffered, with active logging
checking for a periodic flush no more frequently than the configured interval (one second by
default). Every orderly shutdown flushes the file. No flushing thread is created.

`std::cout`, `std::cerr`, and `std::clog` are still mirrored for compatibility. Their individual
buffer writes are synchronized, but chained operations are not guaranteed to form one atomic line
across threads. Prefer a single `Log::*` call for concurrent output.

## Rate limiting

```cpp
if (!Log::isRateLimited("FishActivity:non_water", 5.0)) {
    Log::debug("FishActivity", "Rejected activity outside water");
}
```

The portion before the first colon is the configurable category, so the `FishActivity` JSON value
overrides the fallback `5.0` seconds above. Keys should be stable categories. Do not put entity IDs,
coordinates, frame numbers, timestamps, or other continuously changing values in them. Storage is
bounded and stale entries are periodically removed as a safeguard, but stable keys give correct
suppression behavior.

## Scoped durations

```cpp
Log::ScopedTimer timer(
    Log::Level::Debug,
    "MapTransition",
    "Build mountain vegetation",
    5.0
);
```

The final argument is an optional minimum duration in milliseconds. Disabled levels avoid starting
the timer, and durations below the threshold are not logged.

## Raylib

The logger installs a Raylib trace callback when defaults or JSON configuration are applied.
`LOG_FATAL` and `LOG_ERROR` map to `Error`; the other Raylib levels map directly. Entries use the
`Raylib` tag and still respect `SetTraceLogLevel()` filtering.

## JSON configuration

```json
{
  "logging": {
    "level": "info",
    "raylib_level": "warning",
    "always_show_warnings": true,
    "flush_interval_seconds": 1.0,
    "max_log_files": 50,
    "rate_limit_entry_ttl_seconds": 600.0,
    "max_rate_limit_entries": 2048,
    "rate_limits": {
      "FishActivity": 5.0,
      "Collision": 1.0
    }
  }
}
```

Each configuration load starts from these defaults and replaces the complete rate-limit map, so
removed settings cannot leak across reloads. Invalid properties produce a warning and retain a safe
default.

## Development test

Enable `CPP_GAME_CORE_BUILD_LOG_TEST` in a build where the normal Raylib and nlohmann-json targets
are available, then run `cpp_game_core_log_test`. It covers filtering, timestamps, console teeing,
Raylib capture, rate limiting and cleanup, timer thresholds, retention, repeated lifecycle calls,
configuration reload, and concurrent structured logging.
