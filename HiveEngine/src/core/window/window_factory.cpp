//
// Created by samuel on 9/16/24.
//

#include "window_factory.h"

#include "platform/glfw/glfw_window.h"
#include "platform/raylib/WindowRaylib.h"
#include "window_configuration.h"

hive::Window *hive::WindowFactory::Create(const std::string &title, const int width, const int height,
                                          const WindowConfiguration configuration) {
    return Create(title.c_str(), width, height, configuration);
}

hive::Window * hive::WindowFactory::Create(const char *title, const int width, const int height, const WindowConfiguration configuration) {
#ifdef HIVE_PLATFORM_GLFW
    return new GlfwWindow(title, width, height, configuration);
#endif

#ifdef HIVE_PLATFORM_RAYLIB
	return new WindowRaylib(title, width, height, configuration);
#endif
}
