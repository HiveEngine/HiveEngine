#pragma once

namespace hive
{
    enum class STAGE_LOCATION
    {
        VERTEX, FRAGMENT
    };

    enum class VARIABLE_TYPE
    {
        UNIFORM_BUFFER
    };

    struct ShaderLayout
    {
        VARIABLE_TYPE type;
        STAGE_LOCATION location;
        u32 binding_location;
    };
}