#include <teya/core/Input.h>
#include <teya/core/Profile.h>
#include <array>
#include <raylib.h>

namespace teya::core {
namespace
{
    constexpr int gamepadCount = 4;

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
        case Action::Attack:
            return {KEY_E, KEY_NULL};
        case Action::Confirm:
            return {KEY_ENTER, KEY_SPACE};
        case Action::Cancel:
            return {KEY_ESCAPE, KEY_BACKSPACE};
        case Action::Pause:
            return {KEY_P, KEY_NULL};
        }

        return {KEY_NULL, KEY_NULL};
    }

    constexpr GamepadButton gamepadBindingFor(Action action)
    {
        switch (action)
        {
        case Action::MoveLeft:
            return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        case Action::MoveRight:
            return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
        case Action::MoveUp:
            return GAMEPAD_BUTTON_LEFT_FACE_UP;
        case Action::MoveDown:
            return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
        case Action::Run:
            return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
        case Action::Attack:
            return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case Action::Confirm:
            return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case Action::Cancel:
            return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
        case Action::Pause:
            return GAMEPAD_BUTTON_MIDDLE_RIGHT;
        }

        return GAMEPAD_BUTTON_UNKNOWN;
    }

    using KeyQuery = bool (*)(int key);
    using GamepadQuery = bool (*)(int gamepad, int button);

    bool anyBindingMatches(Action action, KeyQuery keyQuery, GamepadQuery gamepadQuery)
    {
        TEYA_PROFILE_ZONE_NAMED("Input::queryAction");
        for (const KeyboardKey key : bindingsFor(action))
        {
            if (key != KEY_NULL && keyQuery(static_cast<int>(key)))
            {
                return true;
            }
        }

        const GamepadButton button = gamepadBindingFor(action);
        if (button == GAMEPAD_BUTTON_UNKNOWN)
        {
            return false;
        }

        for (int gamepad = 0; gamepad < gamepadCount; ++gamepad)
        {
            if (IsGamepadAvailable(gamepad) && gamepadQuery(gamepad, static_cast<int>(button)))
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

    bool isDown(Action action) { return anyBindingMatches(action, IsKeyDown, IsGamepadButtonDown); }

    bool isPressed(Action action) { return anyBindingMatches(action, IsKeyPressed, IsGamepadButtonPressed); }

    bool isReleased(Action action) { return anyBindingMatches(action, IsKeyReleased, IsGamepadButtonReleased); }

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
