#pragma once

enum class Action
{
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Confirm,
    Cancel,
    Pause
};

namespace Input
{

    [[nodiscard]] bool isDown(Action action);
    [[nodiscard]] bool isPressed(Action action);
    [[nodiscard]] bool isReleased(Action action);

} // namespace Input
