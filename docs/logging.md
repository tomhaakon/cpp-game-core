# Logging

The logger provides timestamped, thread-safe console and file messages. It has no JSON or Raylib
integration and does not intercept standard streams. Parent projects can forward Raylib callbacks
to the normal `Log::*` functions when needed.

The examples below assume:

```cpp
#include <teya/core/Log.h>

using namespace teya::core;
```

## Initialization

```cpp
const auto logDirectory = std::filesystem::path{"logs"};
if (!Log::deleteOldLogs(logDirectory, 10)) return 1;
if (!Log::initialize(logDirectory / "application-2026-08-03_20-51-14.log")) return 1;
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
already open, writes a warning, and returns `false`. Successful initialization returns `true`.
Logging before initialization still writes to the console.

The application is responsible for choosing a unique filename for each run, including any desired
date or run identifier. The logger never checks file size, renames the active file, or rotates it
while messages are being written.

Call `deleteOldLogs(directory, maxFiles)` before `initialize()` to optionally limit stored logs. It
examines regular files with the `.log` extension, sorts them by modification time, and deletes the
oldest files while keeping the requested number of newest files. It ignores other extensions. A
missing directory is considered successful; filesystem inspection or deletion errors return
`false`. Do not call it while a log file in that directory is active.

## Configuration

Pass a `LogConfig` as the optional second argument:

```cpp
Log::LogConfig config;
config.level = Log::Level::Debug;
config.flushIntervalSeconds = 0.5;
config.flushWarnings = true;
config.rateLimitEntryTtlSeconds = 600.0;
config.maxRateLimitEntries = 2048;
if (!Log::initialize("logs/application-2026-08-03_20-51-14.log", config)) {
    // Handle an invalid path or an already initialized logger.
}
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

File and console output use local date and time with milliseconds:

```text
[2026-08-03 20:51:14.382] [INFO] [Startup] Initialization completed
```

Each `Log::*` call is atomic relative to other logger calls. Raw writes to `std::cout`, `std::cerr`,
or `std::clog` are not captured and may interleave with logger output. Prefer `Log::*` for messages
that should reach the file. Carriage returns and newlines in tags or messages are escaped as `\\r`
and `\\n`, ensuring each entry occupies exactly one line.

Errors flush immediately. Warnings flush immediately by default. Lower levels remain buffered and
active logging checks the periodic interval without creating a background thread. `shutdown()`
always writes a `Log session ended normally` divider and flushes the file. Call `Log::flush()` when
an explicit durability point is needed.

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

Configure with `TEYA_CORE_BUILD_TESTS=ON`, build, and run `teya_core_log_test`. The test
exercises filtering, full-date timestamps, explicit flushing, console and file output, newline
escaping, startup cleanup, the absence of stream interception, rate limiting, bounded limiter
cleanup, timer thresholds, repeated lifecycle calls, normal session endings, exact-path behavior,
and concurrent structured logging.
