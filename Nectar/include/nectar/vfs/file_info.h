#pragma once

#include <wax/containers/string.h>

#include <cstddef>
#include <cstdint>

namespace nectar
{
    enum class LoadPriority : uint8_t
    {
        CRITICAL = 0,
        HIGH = 64,
        NORMAL = 128,
        LOW = 192,
        IDLE = 255
    };

    struct FileInfo
    {
        size_t m_size{0};
        bool m_exists{false};
    };

    struct DirectoryEntry
    {
        wax::String m_name;
        bool m_isDirectory{false};
    };
} // namespace nectar
