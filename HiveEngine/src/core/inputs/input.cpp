//
// Created by samuel on 9/16/24.
//

#ifdef HIVE_BACKEND_OPENGL
#include "input.h"

#include <cassert>

#include "input_manager.h"
#include "keycode.h"

#include "core/window/window.h"
#include "platform/glfw/inputs/glfw_input_manager.h"
#include "platform/raylib/InputManagerRaylib.h"

struct InputData {
    hive::InputManager *input_manager;
};

InputData *input_data;

void setupInputManager(hive::WindowNativeData &data)
{
#ifdef HIVE_PLATFORM_RAYLIB
    input_data->input_manager = new hive::InputManagerRaylib();
#endif

#ifdef HIVE_PLATFORM_GLFW
    input_data->input_manager = new hive::GlfwInputManager(static_cast<GLFWwindow*>(data.window_handle));
#endif

}
void hive::Input::init(WindowNativeData window_native_data)
{
    Logger::log("Initializing Input", LogLevel::Debug);
    input_data = new InputData();

    //TODO Add InputManagerFactory
    setupInputManager(window_native_data);
    // switch (window_native_data.backend)
    // {
    // case WindowNativeData::GLFW:
    //     input_data->input_manager = new GlfwInputManager(static_cast<GLFWwindow*>(window_native_data.window_handle));
    //     break;
    // }
}



void hive::Input::shutdown() {
    Logger::log("Shutting down Input", LogLevel::Debug);
    delete input_data->input_manager;
    delete input_data;
}

bool hive::Input::getKey(const KeyCode key_code) {
    assert(input_data->input_manager != nullptr);
    return input_data->input_manager->isKeyDown(getBackendKey(key_code));
}


bool hive::Input::getKeyDown(const KeyCode key_code) {
    assert(input_data->input_manager != nullptr);
    return input_data->input_manager->isKeyPressed(getBackendKey(key_code));
}

bool hive::Input::getKeyUp(const KeyCode key_code) {
    assert(input_data->input_manager != nullptr);
    return input_data->input_manager->isKeyReleased(getBackendKey(key_code));
}
#endif