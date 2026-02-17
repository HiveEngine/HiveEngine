#pragma once

#include <wax/serialization/byte_buffer.h>
#include <wax/containers/string.h>
#include <comb/default_allocator.h>

namespace nectar
{
    struct ImportResult
    {
        bool success{false};
        wax::ByteBuffer<comb::DefaultAllocator> intermediate_data{};
        wax::String<> error_message{};
    };
}
