#include "Application.h"
#include <rendering/Renderer.h>
#include <rendering/RendererFactory.h>

#include <chrono>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

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

static auto startTime = std::chrono::high_resolution_clock::now();
void update_camera(hive::IRenderer &renderer, hive::UniformBufferObjectHandle handle)
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    hive::UniformBufferObject ubo{};
    // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f),
                                1920 / (float) 1080, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;
    renderer.updateUbo(handle, ubo);
}

void hive::Application::run()
{

    auto ubo_handle = renderer_->createUbo();
    ShaderProgramHandle handles[3];
    for (i32 i = 0; i < 3; ++i)
    {
        handles[i] = renderer_->createShader("shaders/vert.spv", "shaders/frag.spv", ubo_handle, i);
    }


    Memory::printMemoryUsage();

    auto begin_time = std::chrono::high_resolution_clock::now();
    i32 current_shader = 0;
    while(!window_.shouldClose())
    {
        window_.pollEvents();

        auto end_time = std::chrono::high_resolution_clock::now();
        if (end_time - begin_time > std::chrono::milliseconds(3000))
        {
            begin_time = end_time;
            current_shader = (current_shader + 1) % 3;
        }
        update_camera(*renderer_, ubo_handle);

        if (!renderer_->beginDrawing()) break; {
            renderer_->useShader(handles[current_shader]);
            renderer_->temp_draw();
        }
        if (!renderer_->endDrawing()) break;
        if (!renderer_->frame()) break;
    }
    renderer_->destroyUbo(ubo_handle);

    for (i32 i = 0; i < 3; ++i)
    {
        renderer_->destroyShader(handles[i]);
    }
}
