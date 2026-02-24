#include <antennae/input.h>
#include <antennae/keyboard.h>
#include <antennae/mouse.h>

#include <queen/world/world.h>
#include <terra/terra.h>

#include <cstring>

namespace antennae
{
    // All keys we poll each frame
    static constexpr terra::Key kPolledKeys[] = {
        terra::Key::Space, terra::Key::Apostrophe, terra::Key::Comma,
        terra::Key::Minus, terra::Key::Period, terra::Key::Slash,
        terra::Key::D0, terra::Key::D1, terra::Key::D2, terra::Key::D3, terra::Key::D4,
        terra::Key::D5, terra::Key::D6, terra::Key::D7, terra::Key::D8, terra::Key::D9,
        terra::Key::Semicolon, terra::Key::Equal,
        terra::Key::A, terra::Key::B, terra::Key::C, terra::Key::D,
        terra::Key::E, terra::Key::F, terra::Key::G, terra::Key::H,
        terra::Key::I, terra::Key::J, terra::Key::K, terra::Key::L,
        terra::Key::M, terra::Key::N, terra::Key::O, terra::Key::P,
        terra::Key::Q, terra::Key::R, terra::Key::S, terra::Key::T,
        terra::Key::U, terra::Key::V, terra::Key::W, terra::Key::X,
        terra::Key::Y, terra::Key::Z,
        terra::Key::LeftBracket, terra::Key::Backslash, terra::Key::RightBracket,
        terra::Key::GraveAccent,
        terra::Key::Escape, terra::Key::Enter, terra::Key::Tab, terra::Key::Backspace,
        terra::Key::Insert, terra::Key::Delete,
        terra::Key::Right, terra::Key::Left, terra::Key::Down, terra::Key::Up,
        terra::Key::PageUp, terra::Key::PageDown, terra::Key::Home, terra::Key::End,
        terra::Key::CapsLock, terra::Key::ScrollLock, terra::Key::NumLock,
        terra::Key::PrintScreen, terra::Key::Pause,
        terra::Key::F1, terra::Key::F2, terra::Key::F3, terra::Key::F4,
        terra::Key::F5, terra::Key::F6, terra::Key::F7, terra::Key::F8,
        terra::Key::F9, terra::Key::F10, terra::Key::F11, terra::Key::F12,
        terra::Key::LeftShift, terra::Key::LeftControl, terra::Key::LeftAlt, terra::Key::LeftSuper,
        terra::Key::RightShift, terra::Key::RightControl, terra::Key::RightAlt, terra::Key::RightSuper,
    };

    static constexpr terra::MouseButton kPolledButtons[] = {
        terra::MouseButton::Left,
        terra::MouseButton::Right,
        terra::MouseButton::Middle,
    };

    void UpdateInput(queen::World& world, terra::WindowContext* window)
    {
        // Insert resources on first call
        if (!world.HasResource<Keyboard>())
            world.InsertResource(Keyboard{});
        if (!world.HasResource<Mouse>())
            world.InsertResource(Mouse{});

        auto* kb = world.Resource<Keyboard>();
        auto* mouse = world.Resource<Mouse>();

        terra::InputState* input = terra::GetWindowInputState(window);

        // Keyboard: previous <- current, then poll
        std::memcpy(kb->previous, kb->current, sizeof(kb->current));
        for (terra::Key key : kPolledKeys)
            kb->current[static_cast<int>(key)] = input->keys_[static_cast<int>(key)];

        // Mouse: previous buttons <- current
        std::memcpy(mouse->prev_buttons, mouse->buttons, sizeof(mouse->buttons));

        // Position + delta (use InputState mouse position and delta)
        float new_x = input->mouseX;
        float new_y = input->mouseY;

        if (mouse->first_update)
        {
            mouse->dx = 0.f;
            mouse->dy = 0.f;
            mouse->first_update = false;
        }
        else
        {
            mouse->dx = new_x - mouse->x;
            mouse->dy = new_y - mouse->y;
        }
        mouse->x = new_x;
        mouse->y = new_y;

        // Buttons
        for (terra::MouseButton btn : kPolledButtons)
            mouse->buttons[static_cast<int>(btn)] = input->mouseButton_[static_cast<int>(btn)];

        // TODO: Terra InputState does not expose scroll delta yet.
        // Restore scroll support once terra::InputState adds scroll fields.
        mouse->scroll_x = 0.f;
        mouse->scroll_y = 0.f;
    }
}
