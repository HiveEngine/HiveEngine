//
// Created by GuillaumeIsCoding on 7/28/2024.
//
#include "linux_mouse.h"

namespace platform
{
	LinuxMouse::LinuxMouse(void* window, const core::MouseConfigurations& configuration) noexcept
	{
		initialize(window, configuration);
	}

	void LinuxMouse::initialize(void* window, const core::MouseConfigurations& configuration) noexcept
	{
		window_ = static_cast<GLFWwindow*>(window);

		setConfiguration(configuration);

		glfwSetCursorPosCallback(window_, LinuxMouse::positionCallback);
		glfwSetMouseButtonCallback(window_, LinuxMouse::buttonCallback);
		glfwSetScrollCallback(window_, LinuxMouse::scrollCallback);
	}

	void LinuxMouse::positionCallback(GLFWwindow* window, double x_position, double y_position) noexcept
	{
		LinuxMouse::data_.x_position = x_position * LinuxMouse::data_.sensitivity;
		LinuxMouse::data_.y_position = y_position * LinuxMouse::data_.sensitivity;
	}

	void LinuxMouse::scrollCallback(GLFWwindow* window, double x_offset, double y_offset) noexcept
	{
		LinuxMouse::data_.x_offset += x_offset * LinuxMouse::data_.sensitivity;
		LinuxMouse::data_.y_offset += y_offset * LinuxMouse::data_.sensitivity;
	}

	void LinuxMouse::buttonCallback(GLFWwindow* window, int button_value, int action, int mods) noexcept
	{
		if (button_value >= static_cast<int>(core::ButtonValue::BUTTON_1) && button_value < static_cast<int>(core::ButtonValue::BUTTON_8)) LinuxMouse::data_.buttons[button_value] = (action == GLFW_PRESS);
	}

	void LinuxMouse::setSensitivity(const float& sensitivity)
	{
		LinuxMouse::data_.sensitivity = (LinuxMouse::data_.sensitivity != sensitivity) ? sensitivity : LinuxMouse::data_.sensitivity;
	}

	void LinuxMouse::setConfiguration(const core::MouseConfigurations& configuration)
	{
		switch (configuration)
		{
		case core::MouseConfigurations::LOCK:
			glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			break;
		case core::MouseConfigurations::HIDDEN:
			glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
			break;
		default:
			glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		}
	}

	void LinuxMouse::getPosition(double& x_position, double& y_position)
	{
		x_position = LinuxMouse::data_.x_position;
		y_position = LinuxMouse::data_.y_position;
	}

	bool LinuxMouse::isButtonPressed(const core::ButtonValue& button_value) const
	{
		return LinuxMouse::data_.buttons[static_cast<int>(button_value)];
	}
}