//
// Created by samuel on 10/16/24.
//
#include "core/inputs/input_manager.h"
#ifdef HIVE_PLATFORM_RAYLIB
#ifndef INPUTMANAGERRAYLIB_H
#define INPUTMANAGERRAYLIB_H

namespace hive
{
	class InputManagerRaylib : public InputManager
	{
	public:
		~InputManagerRaylib() override;
		bool isKeyDown(int key) const override;
		bool isKeyUp(int key) const override;
		bool isKeyPressed(int key) override;
		bool isKeyReleased(int key) override;
		bool isMouseButtonDown(int button) const override;
		bool isMouseButtonUp(int button) const override;
		double getMouseX() const override;
		double getMouseY() const override;
	};

}



#endif //INPUTMANAGERRAYLIB_H
#endif