#include <testbed/precomp.h>
#include <testbed/logtestbed.h>

#include <hive/core/log.h>
#include <hive/core/moduleregistry.h>
#include <swarm/commandpool.h>

#include <terra/window/window.h>

#include <swarm/device.h>
#include <swarm/instance.h>
#include <swarm/program.h>
#include <swarm/shader.h>
#include <swarm/surface.h>
#include <swarm/swapchain.h>
#include <swarm/renderpass.h>
#include <swarm/commandpool.h>
#include <swarm/texture.h>


extern void RegisterSystemModule();

swarm::SurfaceDescription ConvertNativeHandle(const terra::Window::NativeHandle &handle)
{
    swarm::SurfaceDescription result{};
    result.displayHandle = handle.displayHandle;
    result.windowHandle = handle.windowHandle;
    result.windowId = handle.windowId;

    switch (handle.sessionType)
    {
        case terra::Window::NativeHandle::SessionType::NONE:
            break;
        case terra::Window::NativeHandle::SessionType::WAYLAND:
            result.type = swarm::SurfaceDescription::SessionType::WAYLAND;
            break;
        case terra::Window::NativeHandle::SessionType::X11:
            result.type = swarm::SurfaceDescription::SessionType::X11;
            break;
        case terra::Window::NativeHandle::SessionType::WINDOWS:
            result.type = swarm::SurfaceDescription::SessionType::WIN32;
            break;
    }

    return result;
}
int main()
{
    hive::ModuleRegistry moduleRegistry;
    RegisterSystemModule();

    moduleRegistry.CreateModules();
    moduleRegistry.ConfigureModules();
    moduleRegistry.InitModules();

    hive::LogInfo(hive::LogHiveRoot, "Hello from hive");
    hive::LogInfo(LogTestbedRoot, "Hello from testbed");

    if (terra::Window::BackendInitialize())
    {
        //Scope here so the destructor of terra::Window is called before shutting down the backend
        terra::WindowDescription description{"Testbed", 100, 100};
        terra::Window window{description};

        const terra::Window::NativeHandle handle = window.GetNativeHandle();

        swarm::Instance instance{{"Testbed", 0, true}};
        swarm::Surface surface{instance, ConvertNativeHandle(handle)};
        swarm::Device device{instance, surface, {}};
        swarm::Swapchain swapchain{device, {surface}};
        swarm::RenderPass renderpass(device, {swapchain});
        swarm::CommandPool commandpool(device, surface);

        swarm::Shader vertexShader(device, "shaders/vert.spv", {swarm::ShaderStage::VERTEX});
        swarm::Shader fragShader(device, "shaders/frag.spv", {swarm::ShaderStage::FRAGMENT});

        swarm::Program program(device, vertexShader, fragShader, renderpass);

        unsigned int swapchainWidth, swapchainHeight;
        swapchain.GetDimensions(swapchainWidth, swapchainHeight);
        swarm::Texture depthTexture{device, {swapchainWidth, swapchainHeight, swarm::TextureType::DEPTH}};

        while (!window.ShouldClose())
        {
            terra::Window::PollEvents();
        }
    }

    terra::Window::BackendShutdown();

    moduleRegistry.ShutdownModules();
}
