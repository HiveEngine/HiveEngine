#pragma once

#include <nectar/vfs/file_info.h>
#include <wax/containers/string.h>
#include <wax/serialization/byte_buffer.h>
#include <cstdint>

namespace nectar
{
    using IORequestId = uint32_t;
    static constexpr IORequestId kInvalidIORequestId = UINT32_MAX;

    struct IORequest
    {
        IORequestId id{kInvalidIORequestId};
        wax::String<> path;
        LoadPriority priority{LoadPriority::Normal};
        bool cancelled{false};
    };

    struct IOCompletion
    {
        IORequestId request_id{kInvalidIORequestId};
        wax::ByteBuffer<> data;
        bool success{false};
    };
}
