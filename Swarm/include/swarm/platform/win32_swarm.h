#pragma once
#include <Windows.h>
namespace swarm
{
    bool InitRenderContextWin32(RenderContext *renderContext, HINSTANCE instance, HWND window);
}