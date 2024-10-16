//
// Created by samuel on 10/16/24.
//

#ifdef HIVE_PLATFORM_RAYLIB
#include "WindowRaylib.h"

#include <assert.h>

#include "raylib.h"



hive::WindowRaylib::WindowRaylib(const std::string& title, int width, int height, WindowConfiguration configuration) : m_configuration(configuration)
{
	SetTraceLogCallback([](int msgType, const char* message, va_list args) -> void
	{
		switch (msgType)
		{
		case 2:
			Logger::log(std::string(message), LogLevel::Debug);
			break;
		case 3:
			Logger::log(std::string(message), LogLevel::Info);
			break;
		case 4:
			Logger::log(std::string(message), LogLevel::Warning);
			break;
		case 5:
			Logger::log(std::string(message), LogLevel::Error);
			break;
		case 6:
			Logger::log(std::string(message), LogLevel::Fatal);
			break;
		default:
			break;
		}
	});
	SetTargetFPS(60);
	InitWindow(width, height, title.c_str());

}

hive::WindowRaylib::~WindowRaylib()
{
	CloseWindow();
}

int hive::WindowRaylib::getHeight() const
{
	return GetScreenHeight();
}

hive::WindowNativeData hive::WindowRaylib::getNativeWindowData() const
{
	WindowNativeData windowNativeData {GetWindowHandle(), WindowNativeData::RAYLIB};
	return windowNativeData;
}

int hive::WindowRaylib::getWidth() const
{
	return GetScreenWidth();
}

void hive::WindowRaylib::onUpdate() const
{
	PollInputEvents();
	SwapScreenBuffer();
}

void hive::WindowRaylib::setIcon(unsigned char* data, int width, int height) const
{
	//TODO
}

bool hive::WindowRaylib::shouldClose() const
{
	return WindowShouldClose();
}

void hive::WindowRaylib::updateConfiguration(WindowConfiguration configuration)
{
	//TODO
}

hive::WindowConfiguration hive::WindowRaylib::getConfiguration()
{
	return m_configuration;
}

#endif