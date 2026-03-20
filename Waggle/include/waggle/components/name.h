#pragma once

#include <wax/containers/fixed_string.h>

#include <queen/reflect/component_reflector.h>

namespace waggle
{

    struct Name
    {
        wax::FixedString m_name{};

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("name", &Name::m_name);
        }
    };

} // namespace waggle
