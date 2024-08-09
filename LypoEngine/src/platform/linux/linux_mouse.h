//
// Created by GuillaumeIsCoding on 7/28/2024.
//
#pragma once

#include "core/mouse.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace platform
{
	/**
	 * @brief Linux concrete class of core::Mouse
	 */
	class LinuxMouse : public core::Mouse
	{
	public:
		LinuxMouse(void* window, const core::MouseConfigurations& configuration = core::MouseConfigurations::DEFAULT) noexcept;
		virtual ~LinuxMouse() noexcept override = default;

		void setSensitivity(const float& sensitivity) override;
		void setConfiguration(const core::MouseConfigurations& configuration) override;

		void getPosition(double& x_position, double& y_position) override;

		bool isButtonPressed(const core::ButtonValue& value) const override;
	private:
		void initialize(void* window, const core::MouseConfigurations& configuration) noexcept;

		static void positionCallback(GLFWwindow* window, double x_position, double y_position) noexcept;
		static void scrollCallback(GLFWwindow* window, double x_offset, double y_offset) noexcept;
		static void buttonCallback(GLFWwindow* window, int button, int action, int mods) noexcept;
	private:
		struct DataImpl;
		static std::unique_ptr<DataImpl> p_data_impl_;
	};
}