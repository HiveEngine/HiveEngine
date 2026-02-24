#pragma once
#include <swarm/swarm.h>
#include <Windows.h>
namespace swarm
{
    bool InitRenderContextWin32(RenderContext* renderContext, HINSTANCE instance, HWND window,
                                uint32_t width, uint32_t height);
}