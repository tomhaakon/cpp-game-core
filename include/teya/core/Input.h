#pragma once

namespace teya::core {

enum class Action {
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Run,
    Attack,
    Confirm,
    Cancel,
    Pause
};

enum class PointerButton { Primary, Secondary, Middle };

struct PointerPosition {
    float x = 0.0f;
    float y = 0.0f;
};

namespace Input {
[[nodiscard]] bool isDown(Action action);
[[nodiscard]] bool isPressed(Action action);
[[nodiscard]] bool isReleased(Action action);
[[nodiscard]] bool isDown(PointerButton button);
[[nodiscard]] bool isPressed(PointerButton button);
[[nodiscard]] bool isReleased(PointerButton button);
[[nodiscard]] PointerPosition pointerPosition();
} // namespace Input

} // namespace teya::core
