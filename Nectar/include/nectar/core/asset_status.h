#pragma once

#include <wax/containers/string.h>
#include <cstdint>

namespace nectar
{
    enum class AssetStatus : uint8_t
    {
        NotLoaded,
        Queued,
        Loading,
        Ready,
        Failed,
        Unloaded
    };

    enum class AssetError : uint8_t
    {
        None,
        FileNotFound,
        LoadFailed,
        OutOfMemory,
        InvalidHandle,
        NoLoader
    };

    struct AssetErrorInfo
    {
        AssetError code{AssetError::None};
        wax::String<> message{};
    };
}
