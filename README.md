# cpp-game-core

## Logging

The logger writes structured messages to the console and to one file chosen by the application:

```cpp
#include <game_core/Log.h>

int main() {
    Log::initialize("application.log");
    Log::info("Startup", "Application started");
    Log::warning("Configuration", "Optional value is missing");
    Log::shutdown();
}
```

The file is appended across runs. Each initialization adds a timestamped session divider so logs
from different runs remain easy to compare.

Configure it with a small C++ value when defaults are not suitable:

```cpp
Log::LogConfig config;
config.level = Log::Level::Debug;
config.flushIntervalSeconds = 0.5;
Log::initialize("logs/application.log", config);
```

See [the logging guide](docs/logging.md) for levels, rate limiting, timers, and flushing behavior.
