#include <queen/world/world.h>

#include <terra/platform/glfw_terra.h>

#include <antennae/actions.h>
#include <antennae/input.h>
#include <antennae/keyboard.h>
#include <antennae/mouse.h>

#include <larvae/larvae.h>

namespace
{

    // =========================================================================
    // Keyboard
    // =========================================================================

    auto t_kb_default = larvae::RegisterTest("Antennae.Keyboard", "default_all_released", []() {
        antennae::Keyboard kb{};
        larvae::AssertTrue(!kb.IsDown(terra::Key::A));
        larvae::AssertTrue(!kb.IsDown(terra::Key::SPACE));
        larvae::AssertTrue(!kb.IsDown(terra::Key::ESCAPE));
    });

    auto t_kb_isdown = larvae::RegisterTest("Antennae.Keyboard", "IsDown", []() {
        antennae::Keyboard kb{};
        kb.current[static_cast<int>(terra::Key::W)] = true;
        larvae::AssertTrue(kb.IsDown(terra::Key::W));
        larvae::AssertTrue(!kb.IsDown(terra::Key::S));
    });

    auto t_kb_just_pressed = larvae::RegisterTest("Antennae.Keyboard", "JustPressed", []() {
        antennae::Keyboard kb{};
        kb.previous[static_cast<int>(terra::Key::A)] = false;
        kb.current[static_cast<int>(terra::Key::A)] = true;
        larvae::AssertTrue(kb.JustPressed(terra::Key::A));
        larvae::AssertTrue(!kb.JustReleased(terra::Key::A));
    });

    auto t_kb_just_released = larvae::RegisterTest("Antennae.Keyboard", "JustReleased", []() {
        antennae::Keyboard kb{};
        kb.previous[static_cast<int>(terra::Key::A)] = true;
        kb.current[static_cast<int>(terra::Key::A)] = false;
        larvae::AssertTrue(!kb.JustPressed(terra::Key::A));
        larvae::AssertTrue(kb.JustReleased(terra::Key::A));
    });

    auto t_kb_held = larvae::RegisterTest("Antennae.Keyboard", "held_not_just_pressed", []() {
        antennae::Keyboard kb{};
        kb.previous[static_cast<int>(terra::Key::W)] = true;
        kb.current[static_cast<int>(terra::Key::W)] = true;
        larvae::AssertTrue(kb.IsDown(terra::Key::W));
        larvae::AssertTrue(!kb.JustPressed(terra::Key::W));
        larvae::AssertTrue(!kb.JustReleased(terra::Key::W));
    });

    // =========================================================================
    // Mouse
    // =========================================================================

    auto t_mouse_default = larvae::RegisterTest("Antennae.Mouse", "default_zero", []() {
        antennae::Mouse m{};
        larvae::AssertFloatEqual(m.x, 0.f);
        larvae::AssertFloatEqual(m.y, 0.f);
        larvae::AssertFloatEqual(m.dx, 0.f);
        larvae::AssertFloatEqual(m.dy, 0.f);
        larvae::AssertFloatEqual(m.scroll_x, 0.f);
        larvae::AssertFloatEqual(m.scroll_y, 0.f);
        larvae::AssertTrue(!m.IsDown(terra::MouseButton::LEFT));
    });

    auto t_mouse_isdown = larvae::RegisterTest("Antennae.Mouse", "button_IsDown", []() {
        antennae::Mouse m{};
        m.buttons[static_cast<int>(terra::MouseButton::LEFT)] = true;
        larvae::AssertTrue(m.IsDown(terra::MouseButton::LEFT));
        larvae::AssertTrue(!m.IsDown(terra::MouseButton::RIGHT));
    });

    auto t_mouse_just_pressed = larvae::RegisterTest("Antennae.Mouse", "button_JustPressed", []() {
        antennae::Mouse m{};
        m.prev_buttons[static_cast<int>(terra::MouseButton::RIGHT)] = false;
        m.buttons[static_cast<int>(terra::MouseButton::RIGHT)] = true;
        larvae::AssertTrue(m.JustPressed(terra::MouseButton::RIGHT));
        larvae::AssertTrue(!m.JustReleased(terra::MouseButton::RIGHT));
    });

    auto t_mouse_just_released = larvae::RegisterTest("Antennae.Mouse", "button_JustReleased", []() {
        antennae::Mouse m{};
        m.prev_buttons[static_cast<int>(terra::MouseButton::LEFT)] = true;
        m.buttons[static_cast<int>(terra::MouseButton::LEFT)] = false;
        larvae::AssertTrue(!m.JustPressed(terra::MouseButton::LEFT));
        larvae::AssertTrue(m.JustReleased(terra::MouseButton::LEFT));
    });

    auto t_mouse_first_update = larvae::RegisterTest("Antennae.Mouse", "first_update_flag", []() {
        antennae::Mouse m{};
        larvae::AssertTrue(m.first_update);
    });

    // =========================================================================
    // Actions
    // =========================================================================

    auto t_actions_default_bindings = larvae::RegisterTest("Antennae.Actions", "default_bindings", []() {
        antennae::Keyboard keyboard{};
        antennae::Mouse mouse{};
        antennae::InputActionMap actionMap{};
        antennae::InputActions actions{};

        keyboard.current[static_cast<int>(terra::Key::A)] = true;
        antennae::UpdateInputActions(actions, actionMap, keyboard, mouse);
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::MOVE_LEFT));
        larvae::AssertTrue(actions.JustPressed(antennae::InputAction::MOVE_LEFT));

        keyboard.previous[static_cast<int>(terra::Key::A)] = true;
        keyboard.current[static_cast<int>(terra::Key::A)] = false;
        keyboard.current[static_cast<int>(terra::Key::LEFT)] = true;
        antennae::UpdateInputActions(actions, actionMap, keyboard, mouse);
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::MOVE_LEFT));
        larvae::AssertTrue(actions.IsHeld(antennae::InputAction::MOVE_LEFT));

        keyboard.current[static_cast<int>(terra::Key::LEFT)] = false;
        mouse.buttons[static_cast<int>(terra::MouseButton::LEFT)] = true;
        antennae::UpdateInputActions(actions, actionMap, keyboard, mouse);
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::CONFIRM));
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::PRIMARY));
        larvae::AssertTrue(actions.JustPressed(antennae::InputAction::CONFIRM));
    });

    auto t_actions_update_input_wrapper =
        larvae::RegisterTest("Antennae.Actions", "update_input_inserts_resources", []() {
            queen::World world{};
            terra::WindowContext window{};

            window.m_currentInputState.m_keys[static_cast<int>(terra::Key::A)] = true;
            antennae::UpdateInput(world, &window);

            larvae::AssertTrue(world.HasResource<antennae::Keyboard>());
            larvae::AssertTrue(world.HasResource<antennae::Mouse>());
            larvae::AssertTrue(world.HasResource<antennae::InputActionMap>());
            larvae::AssertTrue(world.HasResource<antennae::InputActions>());

            const auto* actions = world.Resource<antennae::InputActions>();
            larvae::AssertTrue(actions != nullptr);
            larvae::AssertTrue(actions->IsDown(antennae::InputAction::MOVE_LEFT));
            larvae::AssertTrue(actions->JustPressed(antennae::InputAction::MOVE_LEFT));

            antennae::InputActionMap* actionMap = world.Resource<antennae::InputActionMap>();
            larvae::AssertTrue(actionMap != nullptr);
            actionMap->Clear(antennae::InputAction::MOVE_LEFT);
            actionMap->AddKey(antennae::InputAction::MOVE_LEFT, terra::Key::D);

            window.m_currentInputState.m_keys[static_cast<int>(terra::Key::A)] = false;
            window.m_currentInputState.m_keys[static_cast<int>(terra::Key::D)] = true;
            antennae::UpdateInput(world, &window);

            larvae::AssertTrue(actions->IsDown(antennae::InputAction::MOVE_LEFT));
            larvae::AssertTrue(actions->IsHeld(antennae::InputAction::MOVE_LEFT));
            larvae::AssertTrue(!actions->JustPressed(antennae::InputAction::MOVE_LEFT));

            window.m_currentInputState.m_keys[static_cast<int>(terra::Key::D)] = false;
            antennae::UpdateInput(world, &window);
            larvae::AssertTrue(actions->JustReleased(antennae::InputAction::MOVE_LEFT));
        });

} // namespace
