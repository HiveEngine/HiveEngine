#include <queen/world/world.h>

#include <terra/terra.h>

#include <antennae/actions.h>
#include <antennae/input.h>
#include <antennae/keyboard.h>
#include <antennae/mouse.h>

#include <cstring>

namespace antennae
{
    // All keys we poll each frame
    static constexpr terra::Key kPolledKeys[] = {
        terra::Key::SPACE,
        terra::Key::APOSTROPHE,
        terra::Key::COMMA,
        terra::Key::MINUS,
        terra::Key::PERIOD,
        terra::Key::SLASH,
        terra::Key::D0,
        terra::Key::D1,
        terra::Key::D2,
        terra::Key::D3,
        terra::Key::D4,
        terra::Key::D5,
        terra::Key::D6,
        terra::Key::D7,
        terra::Key::D8,
        terra::Key::D9,
        terra::Key::SEMICOLON,
        terra::Key::EQUAL,
        terra::Key::A,
        terra::Key::B,
        terra::Key::C,
        terra::Key::D,
        terra::Key::E,
        terra::Key::F,
        terra::Key::G,
        terra::Key::H,
        terra::Key::I,
        terra::Key::J,
        terra::Key::K,
        terra::Key::L,
        terra::Key::M,
        terra::Key::N,
        terra::Key::O,
        terra::Key::P,
        terra::Key::Q,
        terra::Key::R,
        terra::Key::S,
        terra::Key::T,
        terra::Key::U,
        terra::Key::V,
        terra::Key::W,
        terra::Key::X,
        terra::Key::Y,
        terra::Key::Z,
        terra::Key::LEFT_BRACKET,
        terra::Key::BACKSLASH,
        terra::Key::RIGHT_BRACKET,
        terra::Key::GRAVE_ACCENT,
        terra::Key::ESCAPE,
        terra::Key::ENTER,
        terra::Key::TAB,
        terra::Key::BACKSPACE,
        terra::Key::INSERT,
        terra::Key::Delete,
        terra::Key::RIGHT,
        terra::Key::LEFT,
        terra::Key::DOWN,
        terra::Key::UP,
        terra::Key::PAGE_UP,
        terra::Key::PAGE_DOWN,
        terra::Key::HOME,
        terra::Key::END,
        terra::Key::CAPS_LOCK,
        terra::Key::SCROLL_LOCK,
        terra::Key::NUM_LOCK,
        terra::Key::PRINT_SCREEN,
        terra::Key::PAUSE,
        terra::Key::F1,
        terra::Key::F2,
        terra::Key::F3,
        terra::Key::F4,
        terra::Key::F5,
        terra::Key::F6,
        terra::Key::F7,
        terra::Key::F8,
        terra::Key::F9,
        terra::Key::F10,
        terra::Key::F11,
        terra::Key::F12,
        terra::Key::LEFT_SHIFT,
        terra::Key::LEFT_CONTROL,
        terra::Key::LEFT_ALT,
        terra::Key::LEFT_SUPER,
        terra::Key::RIGHT_SHIFT,
        terra::Key::RIGHT_CONTROL,
        terra::Key::RIGHT_ALT,
        terra::Key::RIGHT_SUPER,
    };

    static constexpr terra::MouseButton kPolledButtons[] = {
        terra::MouseButton::LEFT,
        terra::MouseButton::RIGHT,
        terra::MouseButton::MIDDLE,
    };

    void UpdateKeyboard(Keyboard& keyboard, const terra::InputState* input)
    {
        std::memcpy(keyboard.previous, keyboard.current, sizeof(keyboard.current));
        for (terra::Key key : kPolledKeys)
        {
            keyboard.current[static_cast<int>(key)] = terra::IsKeyDown(input, key);
        }
    }

    void UpdateMouse(Mouse& mouse, const terra::InputState* input)
    {
        std::memcpy(mouse.prev_buttons, mouse.buttons, sizeof(mouse.buttons));

        const float newX = terra::GetMouseX(input);
        const float newY = terra::GetMouseY(input);

        if (mouse.first_update)
        {
            mouse.dx = 0.0f;
            mouse.dy = 0.0f;
            mouse.first_update = false;
        }
        else
        {
            mouse.dx = terra::GetMouseDeltaX(input);
            mouse.dy = terra::GetMouseDeltaY(input);
        }
        mouse.x = newX;
        mouse.y = newY;

        for (terra::MouseButton btn : kPolledButtons)
        {
            mouse.buttons[static_cast<int>(btn)] = terra::IsMouseButtonDown(input, btn);
        }

        mouse.scroll_x = 0.0f;
        mouse.scroll_y = 0.0f;
    }

    void UpdateInput(queen::World& world, terra::WindowContext* window)
    {
        if (!world.HasResource<Keyboard>())
            world.InsertResource(Keyboard{});
        if (!world.HasResource<Mouse>())
            world.InsertResource(Mouse{});
        if (!world.HasResource<InputActionMap>())
            world.InsertResource(InputActionMap{});
        if (!world.HasResource<InputActions>())
            world.InsertResource(InputActions{});

        auto* keyboard = world.Resource<Keyboard>();
        auto* mouse = world.Resource<Mouse>();
        auto* actionMap = world.Resource<InputActionMap>();
        auto* actions = world.Resource<InputActions>();

        const terra::InputState* input = terra::GetWindowInputState(window);

        UpdateKeyboard(*keyboard, input);
        UpdateMouse(*mouse, input);
        UpdateInputActions(*actions, *actionMap, *keyboard, *mouse);
    }
} // namespace antennae
