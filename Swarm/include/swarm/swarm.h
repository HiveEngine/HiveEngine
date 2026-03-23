#pragma once

#include <hive/hive_config.h>

#include <cstdint>

namespace terra
{
    struct WindowContext;
}

namespace swarm
{
    // Opaque handle
    struct RenderContext;

    HIVE_API bool InitSystem();
    HIVE_API void ShutdownSystem();

    HIVE_API RenderContext* CreateRenderContext(terra::WindowContext* window);
    HIVE_API void DestroyRenderContext(RenderContext* renderContext);

    HIVE_API void BeginFrame(RenderContext* ctx);
    HIVE_API void EndFrame(RenderContext* ctx);
    HIVE_API void WaitForIdle(RenderContext* ctx);
    HIVE_API void ResizeSwapchain(RenderContext* ctx, uint32_t width, uint32_t height);

    HIVE_API void Render(RenderContext* renderContext);
    HIVE_API void DrawPipeline(RenderContext* ctx);
    HIVE_API void SetupGraphicPipeline(RenderContext& renderContext);

    struct ViewportRT;
    HIVE_API ViewportRT* CreateViewportRT(RenderContext* ctx, uint32_t width, uint32_t height);
    HIVE_API void DestroyViewportRT(ViewportRT* rt);
    HIVE_API void ResizeViewportRT(ViewportRT* rt, uint32_t width, uint32_t height);
    HIVE_API uint32_t GetViewportRTWidth(const ViewportRT* rt);
    HIVE_API uint32_t GetViewportRTHeight(const ViewportRT* rt);
    HIVE_API void* GetViewportRTSRV(const ViewportRT* rt);
    HIVE_API void BeginViewportRT(RenderContext* ctx, ViewportRT* rt);
    HIVE_API void EndViewportRT(RenderContext* ctx, ViewportRT* rt);
} // namespace swarm
