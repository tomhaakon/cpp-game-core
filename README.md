# cpp-game-core

## Logging

The logger writes structured messages to the console and to one file chosen by the application:

```cpp
#include <game_core/Log.h>

int main() {
    if (!Log::deleteOldLogs("logs", 10)) return 1;
    if (!Log::initialize("logs/application-2026-08-03_20-51-14.log")) return 1;
    Log::info("Startup", "Application started");
    Log::warning("Configuration", "Optional value is missing");
    Log::flush(); // optional explicit durability point
    Log::shutdown();
}
```

The application chooses the exact filename, normally including the current date and time. The
logger appends to that file and adds session markers, but never renames or rotates it while running.
`deleteOldLogs()` can remove older `.log` files once during startup.

Configure it with a small C++ value when defaults are not suitable:

```cpp
Log::LogConfig config;
config.level = Log::Level::Debug;
config.flushIntervalSeconds = 0.5;
if (!Log::initialize("logs/application-2026-08-03_20-51-14.log", config)) {
    // Handle an invalid path or an already initialized logger.
}
```

See [the logging guide](docs/logging.md) for levels, rate limiting, timers, and flushing behavior.

## Standalone logging test

The complete `cpp_game_core` library receives raylib and nlohmann/json targets from its parent game. The logger itself is dependency-free and can be built and tested directly on Windows:

```powershell
cmake --preset windows-log-debug
cmake --build --preset windows-log-debug
ctest --preset windows-log-debug
```

In VS Code, open the `cpp-game-core` directory itself and select **Windows Log Test (Visual Studio)** as the configure preset. Opening the template directory configures the template instead.
