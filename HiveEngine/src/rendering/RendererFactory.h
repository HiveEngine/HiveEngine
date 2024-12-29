#pragma once


namespace hive
{
    class IRenderer;
    struct RendererConfig;

    class RendererFactory
    {
    public:
        static IRenderer* createRenderer(const RendererConfig &config);
        static void destroyRenderer(IRenderer* renderer);
    };
}
