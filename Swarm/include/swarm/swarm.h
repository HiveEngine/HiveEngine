#pragma once

#include <cstdint>

namespace swarm
{
    //Opaque handle
    struct RenderContext;

    bool InitSystem();
    void ShutdownSystem();
    void ShutdownRenderContext(RenderContext &renderContext);

    void BeginFrame(RenderContext *ctx);
    void EndFrame(RenderContext *ctx);
    void ResizeSwapchain(RenderContext *ctx, uint32_t width, uint32_t height);

    void Render(RenderContext *renderContext);
    void DrawPipeline(RenderContext *ctx);
    void SetupGraphicPipeline(RenderContext &renderContext);

    struct ViewportRT;
    ViewportRT* CreateViewportRT(RenderContext *ctx, uint32_t width, uint32_t height);
    void DestroyViewportRT(ViewportRT *rt);
    void ResizeViewportRT(ViewportRT *rt, uint32_t width, uint32_t height);
    uint32_t GetViewportRTWidth(const ViewportRT *rt);
    uint32_t GetViewportRTHeight(const ViewportRT *rt);
    void* GetViewportRTSRV(const ViewportRT *rt);
    void BeginViewportRT(RenderContext *ctx, ViewportRT *rt);
    void EndViewportRT(RenderContext *ctx, ViewportRT *rt);
}
