#include <teya/core/Input.h>
#include <array>
#include <raylib.h>

namespace teya::core {
namespace
{

    constexpr std::array<KeyboardKey, 2> bindingsFor(Action action)
    {
        switch (action)
        {
        case Action::MoveLeft:
            return {KEY_A, KEY_LEFT};
        case Action::MoveRight:
            return {KEY_D, KEY_RIGHT};
        case Action::MoveUp:
            return {KEY_W, KEY_UP};
        case Action::MoveDown:
            return {KEY_S, KEY_DOWN};
        case Action::Run:
            return {KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT};
        case Action::Confirm:
            return {KEY_ENTER, KEY_SPACE};
        case Action::Cancel:
            return {KEY_ESCAPE, KEY_BACKSPACE};
        case Action::Pause:
            return {KEY_P, KEY_NULL};
        }

        return {KEY_NULL, KEY_NULL};
    }

    using KeyQuery = bool (*)(int key);

    bool anyBindingMatches(Action action, KeyQuery query)
    {
        for (const KeyboardKey key : bindingsFor(action))
        {
            if (key != KEY_NULL && query(static_cast<int>(key)))
            {
                return true;
            }
        }

        return false;
    }

    MouseButton bindingFor(PointerButton button)
    {
        switch (button)
        {
        case PointerButton::Primary:
            return MOUSE_BUTTON_LEFT;
        case PointerButton::Secondary:
            return MOUSE_BUTTON_RIGHT;
        case PointerButton::Middle:
            return MOUSE_BUTTON_MIDDLE;
        }
        return MOUSE_BUTTON_LEFT;
    }

} // namespace

namespace Input
{

    bool isDown(Action action) { return anyBindingMatches(action, IsKeyDown); }

    bool isPressed(Action action) { return anyBindingMatches(action, IsKeyPressed); }

    bool isReleased(Action action) { return anyBindingMatches(action, IsKeyReleased); }

    bool isDown(PointerButton button)
    {
        return IsMouseButtonDown(static_cast<int>(bindingFor(button)));
    }

    bool isPressed(PointerButton button)
    {
        return IsMouseButtonPressed(static_cast<int>(bindingFor(button)));
    }

    bool isReleased(PointerButton button)
    {
        return IsMouseButtonReleased(static_cast<int>(bindingFor(button)));
    }

    PointerPosition pointerPosition()
    {
        const Vector2 position = GetMousePosition();
        return {position.x, position.y};
    }

} // namespace Input

} // namespace teya::core
