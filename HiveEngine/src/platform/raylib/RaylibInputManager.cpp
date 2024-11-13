//
// Created by mathe on 12/11/2024.
//

#include "RaylibInputManager.h"
#include <raylib.h>


namespace hive {
    bool RaylibInputManager::isKeyDown(const int key) const {
        return IsKeyDown(key);
    }

    bool RaylibInputManager::isKeyUp(const int key) const {
        return IsKeyUp(key);
    }

    bool RaylibInputManager::isKeyPressed(int key) {
        return IsKeyPressed(key);
    }

    bool RaylibInputManager::isKeyReleased(int key) {
        return IsKeyReleased(key);
    }

    bool RaylibInputManager::isMouseButtonDown(int button) const {
        return IsMouseButtonDown(button);
    }

    bool RaylibInputManager::isMouseButtonPressed(int button) const {
        return IsMouseButtonPressed(button);
    }

    bool RaylibInputManager::isMouseButtonUp(int button) const {
        return IsMouseButtonUp(button);
    }

    double RaylibInputManager::getMouseX() const {
        return GetMouseX();
    }

    double RaylibInputManager::getMouseY() const {
        return GetMouseY();
    }
}