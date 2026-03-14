#include <swarm/platform/null_swarm.h>

namespace swarm
{
    bool InitSystem()
    {
        return false;
    }

    void ShutdownSystem() {}

    bool InitRenderContext(RenderContext* /*renderContext*/, terra::WindowContext* /*window*/)
    {
        return false;
    }

    void ShutdownRenderContext(RenderContext& /*renderContext*/) {}

    void BeginFrame(RenderContext* /*ctx*/) {}
    void EndFrame(RenderContext* /*ctx*/) {}
    void WaitForIdle(RenderContext* /*ctx*/) {}
    void ResizeSwapchain(RenderContext* /*ctx*/, uint32_t /*width*/, uint32_t /*height*/) {}

    void Render(RenderContext* /*renderContext*/) {}
    void DrawPipeline(RenderContext* /*ctx*/) {}
    void SetupGraphicPipeline(RenderContext& /*renderContext*/) {}

    ViewportRT* CreateViewportRT(RenderContext* /*ctx*/, uint32_t /*width*/, uint32_t /*height*/)
    {
        return nullptr;
    }

    void DestroyViewportRT(ViewportRT* /*rt*/) {}

    void ResizeViewportRT(ViewportRT* /*rt*/, uint32_t /*width*/, uint32_t /*height*/) {}

    uint32_t GetViewportRTWidth(const ViewportRT* /*rt*/)
    {
        return 0;
    }

    uint32_t GetViewportRTHeight(const ViewportRT* /*rt*/)
    {
        return 0;
    }

    void* GetViewportRTSRV(const ViewportRT* /*rt*/)
    {
        return nullptr;
    }

    void BeginViewportRT(RenderContext* /*ctx*/, ViewportRT* /*rt*/) {}
    void EndViewportRT(RenderContext* /*ctx*/, ViewportRT* /*rt*/) {}
} // namespace swarm
