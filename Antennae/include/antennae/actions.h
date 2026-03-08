#pragma once

#include <antennae/keyboard.h>
#include <antennae/mouse.h>

#include <cstddef>
#include <cstdint>

namespace antennae
{
    enum class InputAction : std::uint8_t
    {
        MOVE_LEFT,
        MOVE_RIGHT,
        MOVE_UP,
        MOVE_DOWN,
        CONFIRM,
        CANCEL,
        PRIMARY,
        SECONDARY,
        COUNT
    };

    enum class DigitalInputType : std::uint8_t
    {
        NONE,
        KEY,
        MOUSE_BUTTON
    };

    struct DigitalInputBinding
    {
        DigitalInputType m_type{DigitalInputType::NONE};
        terra::Key m_key{terra::Key::SPACE};
        terra::MouseButton m_mouseButton{terra::MouseButton::LEFT};

        [[nodiscard]] static DigitalInputBinding FromKey(terra::Key key)
        {
            DigitalInputBinding binding{};
            binding.m_type = DigitalInputType::KEY;
            binding.m_key = key;
            return binding;
        }

        [[nodiscard]] static DigitalInputBinding FromMouseButton(terra::MouseButton button)
        {
            DigitalInputBinding binding{};
            binding.m_type = DigitalInputType::MOUSE_BUTTON;
            binding.m_mouseButton = button;
            return binding;
        }

        [[nodiscard]] bool IsBound() const
        {
            return m_type != DigitalInputType::NONE;
        }

        [[nodiscard]] bool IsDown(const Keyboard& keyboard, const Mouse& mouse) const
        {
            if (m_type == DigitalInputType::KEY)
            {
                return keyboard.IsDown(m_key);
            }

            if (m_type == DigitalInputType::MOUSE_BUTTON)
            {
                return mouse.IsDown(m_mouseButton);
            }

            return false;
        }

        [[nodiscard]] bool JustPressed(const Keyboard& keyboard, const Mouse& mouse) const
        {
            if (m_type == DigitalInputType::KEY)
            {
                return keyboard.JustPressed(m_key);
            }

            if (m_type == DigitalInputType::MOUSE_BUTTON)
            {
                return mouse.JustPressed(m_mouseButton);
            }

            return false;
        }

        [[nodiscard]] bool JustReleased(const Keyboard& keyboard, const Mouse& mouse) const
        {
            if (m_type == DigitalInputType::KEY)
            {
                return keyboard.JustReleased(m_key);
            }

            if (m_type == DigitalInputType::MOUSE_BUTTON)
            {
                return mouse.JustReleased(m_mouseButton);
            }

            return false;
        }
    };

    struct InputActionBinding
    {
        static constexpr size_t kMaxInputs = 4;

        DigitalInputBinding m_inputs[kMaxInputs]{};

        void Clear()
        {
            for (size_t i = 0; i < kMaxInputs; ++i)
            {
                m_inputs[i] = {};
            }
        }

        void AddKey(terra::Key key)
        {
            AddBinding(DigitalInputBinding::FromKey(key));
        }

        void AddMouseButton(terra::MouseButton button)
        {
            AddBinding(DigitalInputBinding::FromMouseButton(button));
        }

        [[nodiscard]] bool IsDown(const Keyboard& keyboard, const Mouse& mouse) const
        {
            for (const auto& input : m_inputs)
            {
                if (input.IsDown(keyboard, mouse))
                {
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] bool JustPressed(const Keyboard& keyboard, const Mouse& mouse) const
        {
            for (const auto& input : m_inputs)
            {
                if (input.JustPressed(keyboard, mouse))
                {
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] bool JustReleased(const Keyboard& keyboard, const Mouse& mouse) const
        {
            for (const auto& input : m_inputs)
            {
                if (input.JustReleased(keyboard, mouse))
                {
                    return true;
                }
            }

            return false;
        }

    private:
        void AddBinding(const DigitalInputBinding& binding)
        {
            for (auto& slot : m_inputs)
            {
                if (!slot.IsBound())
                {
                    slot = binding;
                    return;
                }
            }
        }
    };

    struct InputActionMap
    {
        static constexpr size_t kActionCount = static_cast<size_t>(InputAction::COUNT);

        InputActionBinding m_bindings[kActionCount]{};

        InputActionMap()
        {
            SetDefaults();
        }

        void ClearAll()
        {
            for (auto& binding : m_bindings)
            {
                binding.Clear();
            }
        }

        void Clear(InputAction action)
        {
            Binding(action).Clear();
        }

        void SetDefaults()
        {
            ClearAll();

            AddKey(InputAction::MOVE_LEFT, terra::Key::A);
            AddKey(InputAction::MOVE_LEFT, terra::Key::LEFT);

            AddKey(InputAction::MOVE_RIGHT, terra::Key::D);
            AddKey(InputAction::MOVE_RIGHT, terra::Key::RIGHT);

            AddKey(InputAction::MOVE_UP, terra::Key::W);
            AddKey(InputAction::MOVE_UP, terra::Key::UP);

            AddKey(InputAction::MOVE_DOWN, terra::Key::S);
            AddKey(InputAction::MOVE_DOWN, terra::Key::DOWN);

            AddKey(InputAction::CONFIRM, terra::Key::ENTER);
            AddMouseButton(InputAction::CONFIRM, terra::MouseButton::LEFT);

            AddKey(InputAction::CANCEL, terra::Key::ESCAPE);
            AddMouseButton(InputAction::CANCEL, terra::MouseButton::RIGHT);

            AddMouseButton(InputAction::PRIMARY, terra::MouseButton::LEFT);
            AddMouseButton(InputAction::SECONDARY, terra::MouseButton::RIGHT);
        }

        void AddKey(InputAction action, terra::Key key)
        {
            Binding(action).AddKey(key);
        }

        void AddMouseButton(InputAction action, terra::MouseButton button)
        {
            Binding(action).AddMouseButton(button);
        }

        [[nodiscard]] InputActionBinding& Binding(InputAction action)
        {
            return m_bindings[static_cast<size_t>(action)];
        }

        [[nodiscard]] const InputActionBinding& Binding(InputAction action) const
        {
            return m_bindings[static_cast<size_t>(action)];
        }
    };

    struct InputActions
    {
        static constexpr size_t kActionCount = static_cast<size_t>(InputAction::COUNT);

        bool m_current[kActionCount]{};
        bool m_previous[kActionCount]{};

        [[nodiscard]] bool IsDown(InputAction action) const
        {
            return m_current[static_cast<size_t>(action)];
        }

        [[nodiscard]] bool IsHeld(InputAction action) const
        {
            const size_t index = static_cast<size_t>(action);
            return m_current[index] && m_previous[index];
        }

        [[nodiscard]] bool JustPressed(InputAction action) const
        {
            const size_t index = static_cast<size_t>(action);
            return m_current[index] && !m_previous[index];
        }

        [[nodiscard]] bool JustReleased(InputAction action) const
        {
            const size_t index = static_cast<size_t>(action);
            return !m_current[index] && m_previous[index];
        }
    };

    inline void UpdateInputActions(InputActions& actions, const InputActionMap& actionMap, const Keyboard& keyboard,
                                   const Mouse& mouse)
    {
        for (size_t i = 0; i < InputActions::kActionCount; ++i)
        {
            actions.m_previous[i] = actions.m_current[i];
            actions.m_current[i] = actionMap.m_bindings[i].IsDown(keyboard, mouse);
        }
    }
} // namespace antennae
