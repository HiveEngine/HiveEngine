#pragma once

#include <wax/pointers/handle.h>

#include <cstdint>

namespace nectar
{
    enum class AssetEventKind : uint8_t
    {
        LOADED,   // asset became Ready
        UNLOADED, // asset was garbage collected
        RELOADED, // asset was hot-reloaded with new data
        Failed,   // loading failed
    };

    template <typename T> struct AssetEvent
    {
        AssetEventKind m_kind;
        wax::Handle<T> m_handle;
    };
} // namespace nectar
