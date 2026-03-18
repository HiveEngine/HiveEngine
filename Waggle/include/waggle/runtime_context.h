#pragma once

#include <cstdint>

namespace waggle
{
    enum class EngineMode : uint8_t
    {
        GAME,
        EDITOR,
        HEADLESS
    };

    struct RuntimeContext
    {
        EngineMode m_mode{EngineMode::GAME};
    };
} // namespace waggle
