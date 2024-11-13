//
// Created by mathe on 11/11/2024.
//

#ifdef HIVE_BACKEND_RAYLIB

#include <raylib.h>
#include "core/inputs/mouse.h"

namespace hive
{
    /**
     * Windows concrete class of core::Mouse
     */
    class RaylibMouse : public hive::Mouse
    {
    public:
        RaylibMouse(void* window, const hive::MouseStates& configuration = hive::MouseStates::DEFAULT) noexcept;
        virtual ~RaylibMouse() noexcept override = default;

        void setSensitivity(const float& sensitivity) override;
        void setConfiguration(const hive::MouseStates& configuration) override;

        void getPosition(double& x_position, double& y_position) override;

        bool isButtonPressed(const hive::ButtonValue& value) const override;
        bool isButtonDown(const hive::ButtonValue& value) const override;
    };
}

#endif
