#include "Application.h"

#include <stdexcept>
#include <rendering/Renderer.h>
#include <rendering/RendererFactory.h>

hive::Application::Application(ApplicationConfig &config) : memory_(), window_(config.window_config), renderer_(nullptr)
{
    config.render_config.window = &window_;
    if (!RendererFactory::createRenderer(config.render_config, &renderer_))
    {
        throw std::runtime_error("Failed to create renderer");
    }
}

hive::Application::~Application()
{
    RendererFactory::destroyRenderer(renderer_);
}

void hive::Application::run()
{
    Memory::printMemoryUsage();

    auto shader = renderer_->createProgram("shaders/vert.spv", "shaders/frag.spv");

    while(!window_.shouldClose())
    {
        window_.pollEvents();

        if(renderer_->beginDrawing())
        {
            renderer_->useProgram(shader);
            renderer_->temp_draw();
            renderer_->endDrawing();
            renderer_->frame();
        }
    }

    renderer_->destroyProgram(shader);
}
