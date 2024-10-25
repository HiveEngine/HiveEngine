//
// Created by wstap on 8/4/2024.
//

#ifdef HIVE_BACKEND_OPENGL
#include "glfw_input_manager.h"

#include "core/events/event_bus.h"
#include "core/events/key_event.h"

namespace hive {

    bool GlfwInputManager::isKeyDown(const int key) const
    {
        return glfwGetKey(window_, key) == GLFW_PRESS;
    }

    bool GlfwInputManager::isKeyUp(const int key) const
    {
        return glfwGetKey(window_, key) == GLFW_RELEASE;
    }

    bool GlfwInputManager::isKeyPressed(const int key) {

        const bool pressed = isKeyDown(key) && !keyPressState[key];
        keyPressState[key] = isKeyDown(key);
        return pressed;
    }

    bool GlfwInputManager::isKeyReleased(const int key) {
        const bool released = isKeyUp(key) && !keyReleaseState[key];
        keyReleaseState[key] = isKeyUp(key);
        return released;
    }

    // Mouse inputs
    bool GlfwInputManager::isMouseButtonDown(int button) const
    {
        return glfwGetMouseButton(window_, button) == GLFW_PRESS;
    }

    bool GlfwInputManager::isMouseButtonUp(int button) const
    {
        return glfwGetMouseButton(window_, button) == GLFW_RELEASE;
    }

    // Mouse position
    double GlfwInputManager::getMouseX() const
    {
        double xpos, ypos;
        glfwGetCursorPos(window_, &xpos, &ypos);
        return xpos;
    }

    double GlfwInputManager::getMouseY() const
    {
        double xpos, ypos;
        glfwGetCursorPos(window_, &xpos, &ypos);
        return ypos;
    }



    void GlfwInputManager::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        hive::EventBus& bus = hive::EventBus::getInstance();
        switch (action)
        {
            case GLFW_PRESS:
            {
                hive::KeyPressedEvent event(key, false);
                bus.dispatch(&event);
                break;
            }
            case GLFW_RELEASE:
            {
                hive::KeyReleasedEvent event(key);
                bus.dispatch(&event);
                break;
            }
            case GLFW_REPEAT:
            {
                hive::KeyPressedEvent event(key, true);
                bus.dispatch(&event);
                break;
            }
            default:
                break;
        }
    }
}
#endif