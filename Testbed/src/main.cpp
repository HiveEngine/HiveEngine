#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/detail/func_packing_simd.inl>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <testbed/precomp.h>
#include <testbed/logtestbed.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


#include <hive/core/log.h>
#include <hive/core/moduleregistry.h>

#include <terra/window/window.h>

#include <swarm/swarm.h>
#include <swarm/math.h>
#include <terra/window/window.h>
#include <terra/window/window.h>


#include <iostream>
struct Vertex
{
    swarm::Vec3 position{};
    swarm::Vec3 color{};
    swarm::Vec2 textureCoord{};

    bool operator==(const Vertex& other) const {
        return position == other.position && color == other.color && textureCoord == other.textureCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.textureCoord) << 1);
        }
    };
}

extern void RegisterSystemModule();

swarm::SurfaceCreateInfo ConvertNativeHandle(const terra::Window::NativeHandle &handle)
{
    swarm::SurfaceCreateInfo result{};

    switch (handle.sessionType)
    {
        case terra::Window::NativeHandle::SessionType::NONE:
            break;
        case terra::Window::NativeHandle::SessionType::WAYLAND:
            result.displayReference.waylandDisplay = handle.displayHandle;
            result.surfaceReference.waylandSurface = handle.windowHandle;
            result.type = swarm::SessionType::WAYLAND;
            break;
        case terra::Window::NativeHandle::SessionType::X11:
            result.displayReference.x11Dpy = handle.displayHandle;
            result.surfaceReference.x11WindowId = handle.windowId;
            result.type = swarm::SessionType::X11;
            break;
        case terra::Window::NativeHandle::SessionType::WINDOWS:
            result.displayReference.win32Hinstance = handle.displayHandle;
            result.surfaceReference.win32Hwnd = handle.windowHandle;
            result.type = swarm::SessionType::WIN;
            break;
    }

    return result;
}

struct RenderContext
{
    static constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;
    swarm::InstanceHandle instance{nullptr};
    swarm::DeviceHandle device{nullptr};
    swarm::SurfaceHandle surface{nullptr};
    swarm::SwapchainHandle swapchain{nullptr};
    swarm::RenderpassHandle renderpass{nullptr};
    swarm::TextureHandle depthTexture{nullptr};
    swarm::FramebufferHandle framebuffer{nullptr};
    swarm::CommandPoolHandle commandPool{nullptr};

    std::array<swarm::CommandBufferHandle, MAX_FRAMES_IN_FLIGHT> commandBuffers;
    std::array<swarm::FenceHandle, MAX_FRAMES_IN_FLIGHT> inFlightFences;
    std::array<swarm::SemaphoreHandle, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    std::vector<swarm::SemaphoreHandle> renderFinishedSemaphores;

    //Application specific
    swarm::BufferHandle uniformBuffer{nullptr};

    swarm::ShaderHandle vertexShader{nullptr};
    swarm::ShaderHandle fragmentShader{nullptr};

    swarm::PipelineHandle pipeline{nullptr};
    swarm::DescriptorSetlayoutHandle descriptorSetlayout{nullptr};
};

void InitRenderContext(RenderContext &context, terra::Window::NativeHandle handle);
void InitScene(RenderContext &context);
void ShutdownScene(RenderContext &context);
void ShutdownRenderContext(RenderContext &context);


struct Model
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    swarm::BufferHandle vertexBuffer{nullptr};
    swarm::BufferHandle indexBuffer{nullptr};

    swarm::TextureHandle texture{nullptr};
    swarm::SamplerHandle sampler{nullptr};
};

void LoadModel(RenderContext &context, Model& model);
void DestroyModel(RenderContext &context, Model& model);




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
        terra::WindowDescription description{"Testbed", 900, 720};
        terra::Window window{description};

        const terra::Window::NativeHandle handle = window.GetNativeHandle();

        swarm::InitSwarm();

        RenderContext renderContext{};
        Model model{};
        InitRenderContext(renderContext, handle);
        InitScene(renderContext);
        LoadModel(renderContext, model);

        int frame = 0;
        while (!window.ShouldClose())
        {
            terra::Window::PollEvents();
            swarm::draw(renderContext.device, renderContext.inFlightFences[frame], renderContext.imageAvailableSemaphores[frame], renderContext.renderFinishedSemaphores,
                        renderContext.swapchain, renderContext.commandBuffers[frame], renderContext.renderpass, nullptr, renderContext.framebuffer);
            frame = (frame + 1) % 2;
        }

        DestroyModel(renderContext, model);
        ShutdownScene(renderContext);
        ShutdownRenderContext(renderContext);
    }

    terra::Window::BackendShutdown();
    moduleRegistry.ShutdownModules();
}

void InitRenderContext(RenderContext &context, terra::Window::NativeHandle handle)
{
    swarm::InstanceCreateInfo instanceInfo{"Testbed", 1, true};
    context.instance = swarm::CreateInstance(instanceInfo);

    swarm::SurfaceCreateInfo surfaceInfo = ConvertNativeHandle(handle);
    context.surface = swarm::CreateSurface(context.instance, surfaceInfo);

    swarm::DeviceCreateInfo deviceInfo{};
    context.device = swarm::CreateDevice(context.instance, context.surface, deviceInfo);

    swarm::SwapchainCreateInfo swapchainCreateInfo{};
    context.swapchain = swarm::CreateSwapchain(context.device, swapchainCreateInfo);

    swarm::RenderpassCreateInfo renderpassCreateInfo{};
    context.renderpass = swarm::CreateRenderpass(context.device, context.swapchain, renderpassCreateInfo);

    unsigned int swapchainWidth, swapchainHeight;
    swarm::GetSwapchainExtent(context.swapchain, swapchainWidth, swapchainHeight);

    swarm::TextureCreateInfo depthTextureCreateInfo{};
    depthTextureCreateInfo.type = swarm::TextureType::TEXTURE_2D;
    depthTextureCreateInfo.usage = swarm::TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT;
    depthTextureCreateInfo.format = swarm::TextureFormat::D32_SFLOAT;
    depthTextureCreateInfo.width = swapchainWidth;
    depthTextureCreateInfo.height = swapchainHeight;
    context.depthTexture = swarm::CreateTexture(context.device, depthTextureCreateInfo);

    context.framebuffer = swarm::CreateFramebuffer(context.device, context.swapchain, context.renderpass, context.depthTexture);

    context.commandPool = swarm::CreateCommandPool(context.device);

    context.commandBuffers[0] = swarm::CreateCommandBuffer(context.device, context.commandPool);
    context.commandBuffers[1] = swarm::CreateCommandBuffer(context.device, context.commandPool);

    for (auto& fence : context.inFlightFences)
    {
        fence = swarm::CreateFence(context.device);
    }

    for (auto& semaphore : context.imageAvailableSemaphores)
    {
        semaphore = swarm::CreateSemaphore(context.device);
    }

    context.renderFinishedSemaphores.resize(swarm::GetSwapchainImageCount(context.swapchain));
    for (auto& sem : context.renderFinishedSemaphores)
    {
        sem = swarm::CreateSemaphore(context.device);
    }
}

void ShutdownRenderContext(RenderContext &context)
{
    swarm::WaitDeviceIdle(context.device);

    for (auto& fence : context.inFlightFences)
    {
        swarm::DestroyFence(context.device, fence);
    }

    for (auto& semaphore : context.imageAvailableSemaphores)
    {
        swarm::DestroySemaphore(context.device, semaphore);
    }

    for (auto& semaphore : context.renderFinishedSemaphores)
    {
        swarm::DestroySemaphore(context.device, semaphore);
    }

    swarm::DestroyCommandBuffer(context.device, context.commandPool, context.commandBuffers[0]);
    swarm::DestroyCommandBuffer(context.device, context.commandPool, context.commandBuffers[1]);
    swarm::DestroyCommandPool(context.device, context.commandPool);
    swarm::DestroyFramebuffer(context.device, context.framebuffer);
    swarm::DestroyTexture(context.device, context.depthTexture);
    swarm::DestroyRenderpass(context.device, context.renderpass);
    swarm::DestroySwapchain(context.device, context.swapchain);
    swarm::DestroyDevice(context.device);
    swarm::DestroySurface(context.instance, context.surface);
    swarm::DestroyInstance(context.instance);
}

void InitScene(RenderContext &context)
{
    swarm::ShaderCreateInfo shaderCreateInfo{};
    shaderCreateInfo.path = "shaders/vert.spv";
    shaderCreateInfo.stage = swarm::ShaderStage::VERTEX;
    context.vertexShader = swarm::CreateShader(context.device, shaderCreateInfo);

    shaderCreateInfo.path = "shaders/frag.spv";
    shaderCreateInfo.stage = swarm::ShaderStage::FRAGMENT;
    context.fragmentShader = swarm::CreateShader(context.device, shaderCreateInfo);

    // Create descriptor set layout matching shader bindings:
    // shader.vert: layout(binding = 0) uniform UniformBufferObject -> UBO at binding 0
    // shader.frag: layout(binding = 1) uniform sampler2D texSampler -> IMAGE_SAMPLER at binding 1
    std::vector<swarm::DescriptorSetLayoutBinding> bindings;
    bindings.push_back({0, 1, swarm::BindingType::UBO, swarm::ShaderStage::VERTEX}); // binding 0: UBO
    bindings.push_back({1, 1, swarm::BindingType::IMAGE_SAMPLER, swarm::ShaderStage::FRAGMENT}); // binding 1: sampler2D

    context.descriptorSetlayout = swarm::CreateDescriptorSetlayout(
    context.device, bindings.data(), bindings.size());

    // Create vertex specification matching the shader.vert inputs:
    // Vertex shader expects:
    //   layout(location = 0) in vec3 inPosition;  -> maps to Vertex::position (VEC3 at offset 0)
    //   layout(location = 1) in vec3 inColor;     -> maps to Vertex::color (VEC3 at offset 12)
    //   layout(location = 2) in vec2 inTexCoord;  -> maps to Vertex::textureCoord (VEC2 at offset 24)
    // Total vertex size: 32 bytes (3*4 + 3*4 + 2*4 = 32 bytes)
    swarm::VertexBinding binding = {0, sizeof(Vertex)};
    swarm::VertexAttribute attributes[] = {
    {0, swarm::VertexAttributeType::VEC3, 0}, // location 0: inPosition
    {1, swarm::VertexAttributeType::VEC3, 12}, // location 1: inColor
    {2, swarm::VertexAttributeType::VEC2, 24} // location 2: inTexCoord
    };
    swarm::VertexSpecification vertexSpec = {&binding, 1, attributes, 3};

    swarm::PipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.vertexShader = context.vertexShader;
    pipelineCreateInfo.fragmentShader = context.fragmentShader;
    pipelineCreateInfo.renderpass = context.renderpass;
    pipelineCreateInfo.descriptoSetLayout = context.descriptorSetlayout;
    pipelineCreateInfo.vertexSpec = vertexSpec;
    context.pipeline = swarm::CreatePipeline(context.device, pipelineCreateInfo);

    swarm::BufferCreateInfo uniformBufferCreateInfo{};
    uniformBufferCreateInfo.usage = swarm::BufferUsageFlags::UNIFORM;
    uniformBufferCreateInfo.memoryType = swarm::BufferMemoryType::CPU_TO_GPU;
    uniformBufferCreateInfo.size = sizeof(swarm::Mat4);
    context.uniformBuffer = swarm::CreateBuffer(context.device, uniformBufferCreateInfo);
}

void ShutdownScene(RenderContext &context)
{
    swarm::DestroyBuffer(context.device, context.uniformBuffer);

    swarm::DestroyDescriptorSetlayout(context.device, context.descriptorSetlayout);
    swarm::DestroyPipeline(context.device, context.pipeline);

    swarm::DestroyShader(context.device, context.vertexShader);
    swarm::DestroyShader(context.device, context.fragmentShader);
}

void LoadModel(RenderContext &context, Model& model)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, &err, "./model/viking_room.obj"))
    {
        throw std::runtime_error(err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto &shape: shapes)
    {
        for (const auto &index: shape.mesh.indices)
        {
            Vertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.textureCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(model.vertices.size());
                model.vertices.push_back(vertex);
            }

            model.indices.push_back(uniqueVertices[vertex]);
        }
    }


    swarm::BufferCreateInfo vertexBufferCreateInfo{};
    vertexBufferCreateInfo.usage = swarm::BufferUsageFlags::VERTEX | swarm::BufferUsageFlags::TRANSFER_DST;
    vertexBufferCreateInfo.memoryType = swarm::BufferMemoryType::GPU_ONLY;
    vertexBufferCreateInfo.size = sizeof(Vertex) * model.vertices.size();
    model.vertexBuffer = swarm::CreateBuffer(context.device, vertexBufferCreateInfo);

    swarm::UpdateBuffer(context.device, context.commandPool, model.vertexBuffer, model.vertices.data(), sizeof(Vertex) * model.vertices.size());

    swarm::BufferCreateInfo indexBufferCreateInfo{};
    indexBufferCreateInfo.size = sizeof(uint32_t) * model.indices.size();
    indexBufferCreateInfo.memoryType = swarm::BufferMemoryType::GPU_ONLY;
    indexBufferCreateInfo.usage = swarm::BufferUsageFlags::INDEX | swarm::BufferUsageFlags::TRANSFER_DST;
    model.indexBuffer = swarm::CreateBuffer(context.device, indexBufferCreateInfo);

    swarm::UpdateBuffer(context.device, context.commandPool, model.indexBuffer, model.indices.data(), sizeof(uint32_t) * model.indices.size());

    //Texture
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("./model/viking_room.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    unsigned int imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    swarm::TextureCreateInfo textureCreateInfo{};
    textureCreateInfo.format = swarm::TextureFormat::RGBA8_SRGB;
    textureCreateInfo.usage = swarm::TextureUsageFlags::COLOR_ATTACHMENT | swarm::TextureUsageFlags::SAMPLED;
    textureCreateInfo.type = swarm::TextureType::TEXTURE_2D;
    textureCreateInfo.width = texWidth;
    textureCreateInfo.height = texHeight;

    model.texture = swarm::CreateTexture(context.device, textureCreateInfo);

    swarm::SamplerCreateInfo samplerCreateInfo{};
    model.sampler = swarm::CreateSampler(context.device, samplerCreateInfo);
}

void DestroyModel(RenderContext &context, Model& model)
{
    swarm::DestroySampler(context.device, model.sampler);
    swarm::DestroyTexture(context.device, model.texture);
    swarm::DestroyBuffer(context.device, model.indexBuffer);
    swarm::DestroyBuffer(context.device, model.vertexBuffer);
}



