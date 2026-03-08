#pragma once

#include <wax/containers/string.h>

#include <cstdint>

namespace nectar
{
    enum class AssetStatus : uint8_t
    {
        NOT_LOADED,
        QUEUED,
        LOADING,
        READY,
        Failed,
        UNLOADED
    };

    enum class AssetError : uint8_t
    {
        NONE,
        FILE_NOT_FOUND,
        LOAD_FAILED,
        OUT_OF_MEMORY,
        INVALID_HANDLE,
        NO_LOADER
    };

    struct AssetErrorInfo
    {
        AssetError m_code{AssetError::NONE};
        wax::String m_message{};
    };
} // namespace nectar
