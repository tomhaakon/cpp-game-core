# cpp-game-core

## Logging

The logger writes structured messages to the console and to one file chosen by the application:

```cpp
#include <game_core/Log.h>

int main() {
    Log::initialize("game.log");
    Log::info("Game", "Started");
    Log::warning("Assets", "Missing texture");
    Log::shutdown();
}
```

Configure it with a small C++ value when defaults are not suitable:

```cpp
Log::LogConfig config;
config.level = Log::Level::Debug;
config.flushIntervalSeconds = 0.5;
Log::initialize("logs/game.log", config);
```

See [the logging guide](docs/logging.md) for levels, rate limiting, timers, and flushing behavior.
