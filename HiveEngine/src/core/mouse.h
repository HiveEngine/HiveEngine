//
// Created by GuillaumeIsCoding on 7/28/2024.
//
#pragma once

#include <memory>


namespace hive
{
	/**
	 * @brief Possible mouse configuration
	 */
	enum class MouseStates
	{
		DEFAULT,
		LOCK,
		HIDDEN,
	};

	/**
	 * @brief Value assigns to each mouse button
	 */
	enum class ButtonValue
	{
		BUTTON_1,
		BUTTON_2,
		BUTTON_3,
		BUTTON_4,
		BUTTON_5,
		BUTTON_6,
		BUTTON_7,
		BUTTON_8,
		BUTTON_LAST = BUTTON_8,
		BUTTON_LEFT = BUTTON_1,
		BUTTON_RIGHT = BUTTON_2,
		BUTTON_MIDDLE = BUTTON_3
	};

	/**
	 * @brief API to make the configuration of a Mouse easier
	 */
	class Mouse
	{
	public:
		/**
	 	* @brief Default destructor
	 	*/
		virtual ~Mouse() noexcept = default;

		/**
		 * @brief Sets the sensitivity of the mouse
		 * 
		 * @param sensitivity The sensitivity value to set
		 */
		virtual void setSensitivity(const float& sensitivity) = 0;
		/**
		 * @brief Sets the configuration of the mouse 
		 * 
		 * @param configuration The configuration of the mouse 
		 */
		virtual void setConfiguration(const hive::MouseStates& configuration) = 0;

		/**
		 * @brief Gets the x and y position of the mouse 
		 * 
		 * @param[out] x_position The x position of the mouse
		 * 
		 * @param[out] y_position The y position of the mouse
		 */
		virtual void getPosition(double& x_position, double& y_position) = 0;
		
		/**
		 * @brief Checks if a specefic mouse button is pressed
		 * 
		 * @param button_value The value of the button to check
		 * @return true If the button is pressed.
		 * @return false If the button is not pressed
		 */
		virtual bool isButtonPressed(const hive::ButtonValue& button_value) const = 0;

		/**
		 * 
		 */
		static std::unique_ptr<Mouse> create(void* window, const MouseStates& configuration = hive::MouseStates::DEFAULT);
	protected:
		struct DataImpl; 
	};
}