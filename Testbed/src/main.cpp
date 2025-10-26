#include <glm/detail/func_packing_simd.inl>
#include <glm/glm.hpp>
#include <testbed/precomp.h>
#include <testbed/logtestbed.h>

#include <hive/core/log.h>
#include <hive/core/moduleregistry.h>
// #include <swarm/commandpool.h>

#include <terra/window/window.h>

// #include <swarm/device.h>
// #include <swarm/instance.h>
// #include <swarm/program.h>
// #include <swarm/shader.h>
// #include <swarm/surface.h>
// #include <swarm/swapchain.h>
// #include <swarm/renderpass.h>
// #include <swarm/commandpool.h>
// #include <swarm/framebuffer.h>
// #include <swarm/texture.h>
// #include <swarm/buffer.h>
#include <swarm/swarm.h>


extern void RegisterSystemModule();

swarm::SurfaceCreateInfo ConvertNativeHandle(const terra::Window::NativeHandle &handle)
{
    swarm::SurfaceCreateInfo result{};
    result.displayReference.waylandDisplay = handle.displayHandle;
    result.surfaceReference.waylandSurface = handle.windowHandle;

    switch (handle.sessionType)
    {
        case terra::Window::NativeHandle::SessionType::NONE:
            break;
        case terra::Window::NativeHandle::SessionType::WAYLAND:
            result.type = swarm::SessionType::WAYLAND;
            break;
        case terra::Window::NativeHandle::SessionType::X11:
            result.type = swarm::SessionType::X11;
            break;
        case terra::Window::NativeHandle::SessionType::WINDOWS:
            result.type = swarm::SessionType::WIN;
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

        swarm::InitSwarm();

        swarm::InstanceCreateInfo instanceInfo{"Testbed", 1, true};
        swarm::InstanceHandle instance = swarm::CreateInstance(instanceInfo);

        swarm::SurfaceCreateInfo surfaceInfo = ConvertNativeHandle(handle);
        swarm::SurfaceHandle surface = swarm::CreateSurface(instance, surfaceInfo);

        swarm::DeviceCreateInfo deviceInfo{};
        swarm::DeviceHandle device = swarm::CreateDevice(instance, surface, deviceInfo);

        swarm::SwapchainCreateInfo swapchainCreateInfo{};
        swarm::SwapchainHandle swapchain = swarm::CreateSwapchain(device, swapchainCreateInfo);

        swarm::RenderpassCreateInfo renderpassCreateInfo{};
        swarm::RenderpassHandle renderpass = swarm::CreateRenderpass(device, swapchain, renderpassCreateInfo);

        swarm::ShaderCreateInfo shaderCreateInfo{};
        shaderCreateInfo.path = "shaders/vert.spv";
        shaderCreateInfo.stage = swarm::ShaderStage::VERTEX;
        swarm::ShaderHandle vertexShader = swarm::CreateShader(device, shaderCreateInfo);

        shaderCreateInfo.path = "shaders/frag.spv";
        shaderCreateInfo.stage = swarm::ShaderStage::FRAGMENT;
        swarm::ShaderHandle fragmentShader = swarm::CreateShader(device, shaderCreateInfo);

        std::vector<swarm::DescriptorSetLayoutBinding> bindings;
        bindings.push_back({0, 1, swarm::BindingType::UBO, swarm::ShaderStage::VERTEX});
        bindings.push_back({1, 1, swarm::BindingType::IMAGE_SAMPLER, swarm::ShaderStage::FRAGMENT});

        swarm::DescriptorSetlayoutHandle descriptorSetLayout = swarm::CreateDescriptorSetlayout(device, bindings.data(), bindings.size());


        swarm::DestroyDescriptorSetlayout(device, descriptorSetLayout);
        swarm::DestroyShader(device, fragmentShader);
        swarm::DestroyShader(device, vertexShader);
        swarm::DestroyRenderpass(device, renderpass);
        swarm::DestroySwapchain(device, swapchain);
        swarm::DestroyDevice(device);
        swarm::DestroySurface(instance, surface);
        swarm::DestroyInstance(instance);

        // // swarm::CommandPool commandpool(device, surface);
        // //
        // const swarm::Program program(device, vertexShader, fragShader, renderpass);
        // //
        // unsigned int swapchainWidth, swapchainHeight;
        // swapchain.GetDimensions(swapchainWidth, swapchainHeight);
        // swarm::Texture depthTexture{device, {swapchainWidth, swapchainHeight, swarm::TextureType::DEPTH}};
        // swarm::Framebuffer framebuffer{device, {swapchain, depthTexture, renderpass}};
        //
        // struct Vertex
        // {
        //     glm::vec3 position;
        //     glm::vec3 color;
        //     glm::vec3 texCoord;
        // };
        //
        // std::vector<Vertex> vertices;
        // vertices.push_back({});
        // vertices.push_back({});
        // vertices.push_back({});
        // swarm::Buffer vertexBuffer{device, commandpool, {vertices.data(), sizeof(Vertex) * vertices.size(), swarm::BufferType::VERTEX}};
        //
        // while (!window.ShouldClose())
        // {
        //     terra::Window::PollEvents();
        // }
    }

    terra::Window::BackendShutdown();

    moduleRegistry.ShutdownModules();
}
