#include "Application.h"

#include <stdexcept>
#include <rendering/Renderer.h>
#include <rendering/RendererFactory.h>

hive::Application::Application(ApplicationConfig &config) : memory_(), window_(config.window_config), renderer_(window_)
{
    config.render_config.window = &window_;
    // if (!RendererFactory::createRenderer(config.render_config, &renderer_))
    // {
    //     throw std::runtime_error("Failed to create renderer");
    //
    // }
}

hive::Application::~Application()
{
    // RendererFactory::destroyRenderer(renderer_);
}

void hive::Application::run()
{
    Memory::printMemoryUsage();

    while(!window_.shouldClose())
    {
        window_.pollEvents();
        renderer_.drawFrame();

    }
}
