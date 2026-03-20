#include <queen/world/world.h>

#include <larvae/capabilities.h>
#include <terra/platform/glfw_terra.h>

#include <antennae/actions.h>
#include <antennae/input.h>
#include <antennae/keyboard.h>
#include <antennae/mouse.h>

#include <larvae/larvae.h>

namespace
{
    constexpr larvae::CapabilityMask kWindowCapability = larvae::ToMask(larvae::Capability::WINDOW);

    // Keyboard

    auto t_kb_default = larvae::RegisterTestWithCapabilities("Antennae.Keyboard", "default_all_released",
                                                             kWindowCapability, []() {
        antennae::Keyboard kb{};
        larvae::AssertTrue(!kb.IsDown(terra::Key::A));
        larvae::AssertTrue(!kb.IsDown(terra::Key::SPACE));
        larvae::AssertTrue(!kb.IsDown(terra::Key::ESCAPE));
    });

    auto t_kb_isdown = larvae::RegisterTestWithCapabilities("Antennae.Keyboard", "IsDown", kWindowCapability, []() {
        antennae::Keyboard kb{};
        kb.m_current[static_cast<int>(terra::Key::W)] = true;
        larvae::AssertTrue(kb.IsDown(terra::Key::W));
        larvae::AssertTrue(!kb.IsDown(terra::Key::S));
    });

    auto t_kb_just_pressed =
        larvae::RegisterTestWithCapabilities("Antennae.Keyboard", "JustPressed", kWindowCapability, []() {
        antennae::Keyboard kb{};
        kb.m_previous[static_cast<int>(terra::Key::A)] = false;
        kb.m_current[static_cast<int>(terra::Key::A)] = true;
        larvae::AssertTrue(kb.JustPressed(terra::Key::A));
        larvae::AssertTrue(!kb.JustReleased(terra::Key::A));
    });

    auto t_kb_just_released =
        larvae::RegisterTestWithCapabilities("Antennae.Keyboard", "JustReleased", kWindowCapability, []() {
        antennae::Keyboard kb{};
        kb.m_previous[static_cast<int>(terra::Key::A)] = true;
        kb.m_current[static_cast<int>(terra::Key::A)] = false;
        larvae::AssertTrue(!kb.JustPressed(terra::Key::A));
        larvae::AssertTrue(kb.JustReleased(terra::Key::A));
    });

    auto t_kb_held =
        larvae::RegisterTestWithCapabilities("Antennae.Keyboard", "held_not_just_pressed", kWindowCapability, []() {
        antennae::Keyboard kb{};
        kb.m_previous[static_cast<int>(terra::Key::W)] = true;
        kb.m_current[static_cast<int>(terra::Key::W)] = true;
        larvae::AssertTrue(kb.IsDown(terra::Key::W));
        larvae::AssertTrue(!kb.JustPressed(terra::Key::W));
        larvae::AssertTrue(!kb.JustReleased(terra::Key::W));
    });

    // Mouse

    auto t_mouse_default =
        larvae::RegisterTestWithCapabilities("Antennae.Mouse", "default_zero", kWindowCapability, []() {
        antennae::Mouse m{};
        larvae::AssertFloatEqual(m.m_x, 0.f);
        larvae::AssertFloatEqual(m.m_y, 0.f);
        larvae::AssertFloatEqual(m.m_dx, 0.f);
        larvae::AssertFloatEqual(m.m_dy, 0.f);
        larvae::AssertFloatEqual(m.m_scrollX, 0.f);
        larvae::AssertFloatEqual(m.m_scrollY, 0.f);
        larvae::AssertTrue(!m.IsDown(terra::MouseButton::LEFT));
    });

    auto t_mouse_isdown =
        larvae::RegisterTestWithCapabilities("Antennae.Mouse", "button_IsDown", kWindowCapability, []() {
        antennae::Mouse m{};
        m.m_buttons[static_cast<int>(terra::MouseButton::LEFT)] = true;
        larvae::AssertTrue(m.IsDown(terra::MouseButton::LEFT));
        larvae::AssertTrue(!m.IsDown(terra::MouseButton::RIGHT));
    });

    auto t_mouse_just_pressed =
        larvae::RegisterTestWithCapabilities("Antennae.Mouse", "button_JustPressed", kWindowCapability, []() {
        antennae::Mouse m{};
        m.m_prevButtons[static_cast<int>(terra::MouseButton::RIGHT)] = false;
        m.m_buttons[static_cast<int>(terra::MouseButton::RIGHT)] = true;
        larvae::AssertTrue(m.JustPressed(terra::MouseButton::RIGHT));
        larvae::AssertTrue(!m.JustReleased(terra::MouseButton::RIGHT));
    });

    auto t_mouse_just_released =
        larvae::RegisterTestWithCapabilities("Antennae.Mouse", "button_JustReleased", kWindowCapability, []() {
        antennae::Mouse m{};
        m.m_prevButtons[static_cast<int>(terra::MouseButton::LEFT)] = true;
        m.m_buttons[static_cast<int>(terra::MouseButton::LEFT)] = false;
        larvae::AssertTrue(!m.JustPressed(terra::MouseButton::LEFT));
        larvae::AssertTrue(m.JustReleased(terra::MouseButton::LEFT));
    });

    auto t_mouse_first_update =
        larvae::RegisterTestWithCapabilities("Antennae.Mouse", "first_update_flag", kWindowCapability, []() {
        antennae::Mouse m{};
        larvae::AssertTrue(m.m_firstUpdate);
    });

    // Actions

    auto t_actions_default_bindings =
        larvae::RegisterTestWithCapabilities("Antennae.Actions", "default_bindings", kWindowCapability, []() {
        antennae::Keyboard keyboard{};
        antennae::Mouse mouse{};
        antennae::InputActionMap actionMap{};
        antennae::InputActions actions{};

        keyboard.m_current[static_cast<int>(terra::Key::A)] = true;
        antennae::UpdateInputActions(actions, actionMap, keyboard, mouse);
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::MOVE_LEFT));
        larvae::AssertTrue(actions.JustPressed(antennae::InputAction::MOVE_LEFT));

        keyboard.m_previous[static_cast<int>(terra::Key::A)] = true;
        keyboard.m_current[static_cast<int>(terra::Key::A)] = false;
        keyboard.m_current[static_cast<int>(terra::Key::LEFT)] = true;
        antennae::UpdateInputActions(actions, actionMap, keyboard, mouse);
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::MOVE_LEFT));
        larvae::AssertTrue(actions.IsHeld(antennae::InputAction::MOVE_LEFT));

        keyboard.m_current[static_cast<int>(terra::Key::LEFT)] = false;
        mouse.m_buttons[static_cast<int>(terra::MouseButton::LEFT)] = true;
        antennae::UpdateInputActions(actions, actionMap, keyboard, mouse);
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::CONFIRM));
        larvae::AssertTrue(actions.IsDown(antennae::InputAction::PRIMARY));
        larvae::AssertTrue(actions.JustPressed(antennae::InputAction::CONFIRM));
    });

    auto t_actions_update_input_wrapper =
        larvae::RegisterTestWithCapabilities("Antennae.Actions", "update_input_inserts_resources", kWindowCapability,
                                             []() {
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
