#include "Application.h"

#include <rendering/Renderer.h>
#include <rendering/RendererFactory.h>

hive::Application::Application(ApplicationConfig &config) : memory_(), window_(config.window_config)
{
    config.render_config.window = &window_;
    renderer_ = RendererFactory::createRenderer(config.render_config);
}

hive::Application::~Application()
{
    RendererFactory::destroyRenderer(renderer_);
}

void hive::Application::run()
{
    Memory::printMemoryUsage();

    while(!window_.shouldClose())
    {
        window_.pollEvents();
    }
}
