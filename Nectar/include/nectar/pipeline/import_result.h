#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/serialization/byte_buffer.h>

namespace nectar
{
    struct ImportResult
    {
        bool m_success{false};
        wax::ByteBuffer m_intermediateData{};
        wax::String m_errorMessage{};
    };
} // namespace nectar
