#pragma once

#include <wax/pointers/handle.h>
#include <cstdint>

namespace nectar
{
    enum class AssetEventKind : uint8_t
    {
        Loaded,     // asset became Ready
        Unloaded,   // asset was garbage collected
        Reloaded,   // asset was hot-reloaded with new data
        Failed,     // loading failed
    };

    template<typename T>
    struct AssetEvent
    {
        AssetEventKind kind;
        wax::Handle<T> handle;
    };
}
