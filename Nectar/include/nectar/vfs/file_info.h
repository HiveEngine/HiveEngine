#pragma once

#include <wax/containers/string.h>
#include <cstddef>
#include <cstdint>

namespace nectar
{
    enum class LoadPriority : uint8_t
    {
        Critical = 0,
        High     = 64,
        Normal   = 128,
        Low      = 192,
        Idle     = 255
    };

    struct FileInfo
    {
        size_t size{0};
        bool exists{false};
    };

    struct DirectoryEntry
    {
        wax::String<> name;
        bool is_directory{false};
    };
}
