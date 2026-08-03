# Logging

The logger provides timestamped, thread-safe console and file messages. It has no JSON or Raylib
integration and does not intercept standard streams. Parent projects can forward Raylib callbacks
to the normal `Log::*` functions when needed.

## Initialization

```cpp
Log::initialize("application.log");
Log::info("Startup", "Application started");
Log::shutdown();
```

`initialize()` takes the exact output file path and creates its parent directory when necessary.
It appends to an existing file and starts every run with a divider such as:

```text
===== Log session started: 2026-08-03 09:15:42 =====
```

The logger does not scan the directory or create indexed, dated, or process-specific filenames.
Repeated initialization and shutdown calls are safe; initialization is ignored while a file is
already open. Logging before initialization still writes to the console.

## Configuration

Pass a `LogConfig` as the optional second argument:

```cpp
Log::LogConfig config;
config.level = Log::Level::Debug;
config.flushIntervalSeconds = 0.5;
config.flushWarnings = true;
config.rateLimitEntryTtlSeconds = 600.0;
config.maxRateLimitEntries = 2048;
Log::initialize("logs/application.log", config);
```

Defaults:

| Option | Default | Meaning |
| --- | ---: | --- |
| `level` | `Info` | Least-severe enabled level |
| `flushIntervalSeconds` | `1.0` | Activity-driven periodic file flush interval |
| `flushWarnings` | `true` | Flush warnings immediately |
| `rateLimitEntryTtlSeconds` | `600.0` | Remove inactive limiter keys after this duration |
| `maxRateLimitEntries` | `2048` | Maximum limiter keys retained |

Invalid negative or non-finite durations fall back to their defaults. A zero entry limit also falls
back to the default.

## Levels and output

The levels are `Error`, `Warning`, `Info`, `Debug`, and `Trace`. Selecting a level enables it and all
more severe levels.

```cpp
Log::info("Startup", "Initialization completed");
Log::warning("Configuration", "Optional value is missing");
```

Output uses local time with milliseconds:

```text
[20:51:14.382] [INFO] [Startup] Initialization completed
```

Each `Log::*` call is atomic relative to other logger calls. Raw writes to `std::cout`, `std::cerr`,
or `std::clog` are not captured and may interleave with logger output. Prefer `Log::*` for messages
that should reach the file.

Errors flush immediately. Warnings flush immediately by default. Lower levels remain buffered and
active logging checks the periodic interval without creating a background thread. `shutdown()`
always flushes the file.

## Rate limiting

```cpp
if (!Log::isRateLimited("RepeatedEvent", 5.0)) {
    Log::debug("RateLimit", "Repeated event detected");
}
```

Use stable category keys. Do not include entity IDs, coordinates, frame numbers, timestamps, or
other continuously changing values. Stale entries are removed periodically and the map has a hard
size limit as a safeguard.

## Scoped durations

```cpp
Log::ScopedTimer timer(
    Log::Level::Debug,
    "Performance",
    "Process batch",
    5.0
);
```

The optional final argument is the minimum elapsed time in milliseconds. Disabled levels do not
start the timer, and durations below the threshold are not logged.

## Development test

Configure with `CPP_GAME_CORE_BUILD_LOG_TEST=ON`, build, and run `cpp_game_core_log_test`. The test
exercises filtering, timestamps, console and file output, the absence of stream interception, rate
limiting, bounded limiter cleanup, timer thresholds, repeated lifecycle calls, fixed-path behavior,
and concurrent structured logging.
