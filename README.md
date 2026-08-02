# cpp-game-core

## File logging

Full configuration and usage documentation is available in [`docs/logging.md`](docs/logging.md).

Initialize the logger near the start of the game and shut it down before exiting:

```cpp
#include <game_core/Log.h>
#include <iostream>

int main() {
    Log::configureDefaults();
    Log::initializeFileLogging("logs", "My Game");

    Log::info("Startup", "Game initialized");
    std::cout << "This is also copied to the log file\n";

    Log::shutdownFileLogging();
}
```

Each run creates `logs/<index>_<date>_<time>_pid-<process-id>.txt`. Output written to `std::cout`,
`std::cerr`, and `std::clog` is mirrored to that file while logging is active.

Optional JSON configuration can be applied with `Log::configureFromJson(document)`:

```json
{
  "logging": {
    "level": "info",
    "raylib_level": "warning",
    "always_show_warnings": true,
    "rate_limits": {
      "Player.position": 1.0
    }
  }
}
```

Levels are `error`, `warning`, `info`, `debug`, and `trace`. A configured rate-limit
key also applies to detailed keys such as `Player.position:player-1`.
