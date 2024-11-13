//
// Created by mathe on 11/11/2024.
//

#ifdef HIVE_BACKEND_RAYLIB

#include <raylib.h>
#include "platform/raylib/RaylibMouse.h"



namespace hive
{
    RaylibMouse::RaylibMouse(void* window, const hive::MouseStates& configuration) noexcept
    {

    }

    void RaylibMouse::setSensitivity(const float& sensitivity)
    {

    }

    void RaylibMouse::setConfiguration(const hive::MouseStates& configuration)
    {
        switch (configuration)
        {
            case hive::MouseStates::LOCK:
                DisableCursor();
            break;
            case hive::MouseStates::HIDDEN:
                EnableCursor();
                HideCursor();
            break;
            default:
                EnableCursor();
                ShowCursor();
            break;
        }
    }

    void RaylibMouse::getPosition(double& x_position, double& y_position)
    {
        Vector2 mousePosition = GetMousePosition();
        x_position = mousePosition.x;
        y_position = mousePosition.y;
    }

    bool RaylibMouse::isButtonPressed(const hive::ButtonValue& value) const
    {
       return IsMouseButtonPressed(static_cast<int>(value));
    }

    bool RaylibMouse::isButtonDown(const hive::ButtonValue& value) const
    {
        return IsMouseButtonDown(static_cast<int>(value));
    }

}

#endif
