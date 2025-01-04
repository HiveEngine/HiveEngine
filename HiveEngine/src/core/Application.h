#pragma once
#include <optional>
#include <rendering/Renderer.h>
#include <rendering/vulkan/VkRenderer.h>

#include "core/Memory.h"
#include "core/Logger.h"
#include "core/Window.h"

namespace hive
{

    struct ApplicationConfig
    {
        Logger::LogLevel log_level;
        WindowConfig window_config;
        RendererConfig render_config;
    };


    class Application
    {
    public:
        explicit Application(ApplicationConfig &config);

        ~Application();

        void run();


    private:
        Memory memory_;
        Window window_;
        IRenderer* renderer_;
        // vk::VulkanRenderer renderer_;
    };
}
