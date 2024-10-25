//
// Created by mathe on 14/08/2024.
//

#ifdef HIVE_PLATFORM_OPENGL
#include "RenderAPI.h"

namespace hive
{
    RenderAPI::API RenderAPI::api_ = RenderAPI::API::OpenGL;
}
#endif