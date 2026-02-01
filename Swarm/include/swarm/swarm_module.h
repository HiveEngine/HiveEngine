#pragma once
#include <terra/window/window.h>
namespace swarm
{
    void InitVk(terra::Window& window);
    void Shutdown();

    void Render();
}