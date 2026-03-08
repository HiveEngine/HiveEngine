#pragma once

#include <wax/containers/string.h>
#include <wax/serialization/byte_buffer.h>

#include <nectar/vfs/file_info.h>

#include <cstdint>

namespace nectar
{
    using IORequestId = uint32_t;
    static constexpr IORequestId kInvalidIORequestId = UINT32_MAX;

    struct IORequest
    {
        IORequestId m_id{kInvalidIORequestId};
        wax::String m_path;
        LoadPriority m_priority{LoadPriority::NORMAL};
        bool m_cancelled{false};
    };

    struct IOCompletion
    {
        IORequestId m_requestId{kInvalidIORequestId};
        wax::ByteBuffer m_data;
        bool m_success{false};
    };
} // namespace nectar
