//
// Created by samuel on 10/16/24.
//
#ifdef HIVE_PLATFORM_RAYLIB

#ifndef WINDOWRAYLIB_H
#define WINDOWRAYLIB_H

#include "core/window/window.h"

namespace hive
{
	class WindowRaylib : public Window
	{
	public:
		WindowRaylib(const std::string &title, int width, int height, WindowConfiguration configuration);
		~WindowRaylib() override;
		[[nodiscard]] int getHeight() const override;
		[[nodiscard]] WindowNativeData getNativeWindowData() const override;
		[[nodiscard]] int getWidth() const override;
		void onUpdate() const override;
		void setIcon(unsigned char* data, int width, int height) const override;
		[[nodiscard]] bool shouldClose() const override;
		void updateConfiguration(WindowConfiguration configuration) override;
		WindowConfiguration getConfiguration() override;

	private:
		WindowConfiguration m_configuration;
	};
}



#endif //WINDOWRAYLIB_H
#endif