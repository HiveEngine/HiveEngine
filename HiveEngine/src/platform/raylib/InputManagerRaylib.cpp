//
// Created by samuel on 10/16/24.
//
#ifdef HIVE_PLATFORM_RAYLIB
#include "InputManagerRaylib.h"
#include "raylib.h"

hive::InputManagerRaylib::~InputManagerRaylib() = default;

bool hive::InputManagerRaylib::isKeyDown(int key) const
{
	return IsKeyDown(key);
}

bool hive::InputManagerRaylib::isKeyUp(int key) const
{
	return IsKeyUp(key);
}

bool hive::InputManagerRaylib::isKeyPressed(int key)
{
	return IsKeyPressed(key);
}

bool hive::InputManagerRaylib::isKeyReleased(int key)
{
	return IsKeyReleased(key);
}

bool hive::InputManagerRaylib::isMouseButtonDown(int button) const
{
	return IsMouseButtonDown(button);
}

bool hive::InputManagerRaylib::isMouseButtonUp(int button) const
{
	return IsMouseButtonUp(button);
}

double hive::InputManagerRaylib::getMouseX() const
{
	return GetMouseX();
}

double hive::InputManagerRaylib::getMouseY() const
{
	return GetMouseY();
}

#endif