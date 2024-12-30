#pragma once


namespace hive
{
    class IRenderer;
    struct RendererConfig;

    class RendererFactory
    {
    public:
        static bool createRenderer(const RendererConfig &config, IRenderer** out_renderer);
        static void destroyRenderer(IRenderer* renderer);
    };
}
