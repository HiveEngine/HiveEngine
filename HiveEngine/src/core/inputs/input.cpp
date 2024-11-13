//
// Created by samuel on 9/16/24.
//

#include "input.h"
#include "input_manager.h"
#include "keycode.h"
#include "core/window/window.h"
#include "platform/glfw/inputs/glfw_input_manager.h"
#include "platform/raylib/RaylibInputManager.h"

struct InputData {
    hive::InputManager *input_manager;
};

InputData *input_data;

void hive::Input::init(WindowNativeData window_native_data)
{
    Logger::log("Initializing Input", LogLevel::Debug);
    input_data = new InputData();

    #ifdef HIVE_BACKEND_RAYLIB
    input_data->input_manager = new RaylibInputManager();
    #else
    switch (window_native_data.backend)
    {
    case WindowNativeData::GLFW:
        input_data->input_manager = new GlfwInputManager(static_cast<GLFWwindow*>(window_native_data.window_handle));
        break;
    }
    #endif
}

void hive::Input::shutdown() {
    Logger::log("Shutting down Input", LogLevel::Debug);
    delete input_data->input_manager;
    delete input_data;
}

bool hive::Input::getKey(const KeyCode key_code) {
    return input_data->input_manager->isKeyDown(key_code);
}


bool hive::Input::getKeyPressed(const KeyCode key_code) {
    return input_data->input_manager->isKeyPressed(key_code);
}

bool hive::Input::getKeyUp(const KeyCode key_code) {
    return input_data->input_manager->isKeyReleased(key_code);
}

bool hive::Input::getMouseButtonDown(int button) {
    return input_data->input_manager->isMouseButtonDown(button);
}

bool hive::Input::getMouseButtonPressed(int button) {
    return input_data->input_manager->isMouseButtonPressed(button);
}

bool hive::Input::getMouseButtonUp(int button) {
    return input_data->input_manager->isMouseButtonUp(button);
}

double hive::Input::getMouseX() {
    return input_data->input_manager->getMouseX();
}

double hive::Input::getMouseY() {
    return input_data->input_manager->getMouseY();
}






