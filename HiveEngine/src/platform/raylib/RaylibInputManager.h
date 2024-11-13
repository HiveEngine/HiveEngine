//
// Created by mathe on 12/11/2024.
//

#ifndef RAYLIBINPUTMANAGER_H
#define RAYLIBINPUTMANAGER_H

#include "core/inputs/input_manager.h"

namespace hive {
    class RaylibInputManager : public hive::InputManager{
        // Keyboard inputs
        bool isKeyDown(const int key) const override;
        bool isKeyUp(const int key) const override;

        bool isKeyPressed(int key) override;
        bool isKeyReleased(int key) override;

        // Mouse inputs
        bool isMouseButtonDown(int button) const override;
        bool isMouseButtonPressed(int button) const override;
        bool isMouseButtonUp(int button) const override;

        // Mouse position
        double getMouseX() const override;
        double getMouseY() const override;
    };
}



#endif //RAYLIBINPUTMANAGER_H
