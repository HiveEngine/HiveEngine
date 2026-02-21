#pragma once
namespace swarm
{
    //Opaque handle
    struct RenderContext;

    bool InitSystem();
    void ShutdownSystem();
    void ShutdownRenderContext(RenderContext *renderContext);

    void Render(RenderContext *renderContext);

}
