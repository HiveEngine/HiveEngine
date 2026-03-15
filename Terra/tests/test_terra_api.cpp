#include <larvae/capabilities.h>
#include <terra/platform/glfw_terra.h>
#include <terra/terra.h>

#include <larvae/larvae.h>

namespace
{
    auto g_inputHelpersTest = larvae::RegisterTestWithCapabilities(
        "TerraApi", "InputHelpersReadState", larvae::ToMask(larvae::Capability::WINDOW), []() {
        terra::InputState input{};
        input.m_keys[static_cast<int>(terra::Key::A)] = true;
        input.m_mouseButton[static_cast<int>(terra::MouseButton::RIGHT)] = true;
        input.m_mouseX = 42.0f;
        input.m_mouseY = 24.0f;
        input.m_mouseDeltaX = 3.5f;
        input.m_mouseDeltaY = -1.5f;

        larvae::AssertTrue(terra::IsKeyDown(&input, terra::Key::A));
        larvae::AssertFalse(terra::IsKeyDown(&input, terra::Key::B));
        larvae::AssertTrue(terra::IsMouseButtonDown(&input, terra::MouseButton::RIGHT));
        larvae::AssertFalse(terra::IsMouseButtonDown(&input, terra::MouseButton::LEFT));
        larvae::AssertEqual(terra::GetMouseX(&input), 42.0f);
        larvae::AssertEqual(terra::GetMouseY(&input), 24.0f);
        larvae::AssertEqual(terra::GetMouseDeltaX(&input), 3.5f);
        larvae::AssertEqual(terra::GetMouseDeltaY(&input), -1.5f);
    });

    auto g_windowHelpersTest = larvae::RegisterTestWithCapabilities(
        "TerraApi", "WindowHelpersStoreMetadata", larvae::ToMask(larvae::Capability::WINDOW), []() {
        terra::WindowContext window{};

        terra::SetWindowTitle(&window, "Test Window");
        terra::SetWindowSize(&window, 1600, 900);

        larvae::AssertEqual(terra::GetWindowWidth(&window), 1600);
        larvae::AssertEqual(terra::GetWindowHeight(&window), 900);
        larvae::AssertTrue(terra::GetWindowInputState(&window) != nullptr);
        larvae::AssertTrue(terra::GetGlfwWindow(&window) == nullptr);
    });
} // namespace
