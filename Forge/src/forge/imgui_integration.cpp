#include <swarm/platform/diligent_swarm.h>
#include <swarm/swarm.h>

#include <forge/imgui_integration.h>
#include <forge/theme.h>

#include <imgui.h>

#include <ImGuizmo.h>

#include <imgui_impl_glfw.h>

#if HIVE_FEATURE_VULKAN

#include <imgui_impl_vulkan.h>

#include <CommandQueueVk.h>
#include <DeviceContextVk.h>
#include <RenderDeviceVk.h>
#include <TextureViewVk.h>

namespace forge
{
    static VkDevice g_sDevice = VK_NULL_HANDLE;
    static VkDescriptorPool g_sDescriptorPool = VK_NULL_HANDLE;
    static VkRenderPass g_sRenderPass = VK_NULL_HANDLE;

    static constexpr uint32_t kMaxFramesInFlight = 3;
    static VkFramebuffer g_sFramebuffers[kMaxFramesInFlight] = {};
    static uint32_t g_sFrameIdx = 0;

    static VkFormat DiligentToVkFormat(Diligent::TEXTURE_FORMAT fmt) {
        using namespace Diligent;
        switch (fmt)
        {
            case TEX_FORMAT_RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case TEX_FORMAT_RGBA8_UNORM_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case TEX_FORMAT_BGRA8_UNORM:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case TEX_FORMAT_BGRA8_UNORM_SRGB:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case TEX_FORMAT_RGB10A2_UNORM:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            case TEX_FORMAT_RGBA16_FLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            default:
                return VK_FORMAT_UNDEFINED;
        }
    }

    static bool CreateImGuiRenderPass(VkDevice device, VkFormat colorFormat) {
        VkAttachmentDescription att{};
        att.format = colorFormat;
        att.samples = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference ref{};
        ref.attachment = 0;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;

        VkRenderPassCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        ci.attachmentCount = 1;
        ci.pAttachments = &att;
        ci.subpassCount = 1;
        ci.pSubpasses = &subpass;

        return vkCreateRenderPass(device, &ci, nullptr, &g_sRenderPass) == VK_SUCCESS;
    }

    bool ForgeImGuiInit(swarm::RenderContext* ctx, GLFWwindow* window) {
        auto* deviceVk = static_cast<Diligent::IRenderDeviceVk*>(ctx->m_device);

        VkInstance vkInstance = deviceVk->GetVkInstance();
        VkPhysicalDevice vkPhysDev = deviceVk->GetVkPhysicalDevice();
        VkDevice vkDevice = deviceVk->GetVkDevice();

        g_sDevice = vkDevice;

        // Queue info
        auto* cmdQueue = ctx->m_context->LockCommandQueue();
        auto* cmdQueueVk = static_cast<Diligent::ICommandQueueVk*>(cmdQueue);
        VkQueue vkQueue = cmdQueueVk->GetVkQueue();
        uint32_t queueFamily = cmdQueueVk->GetQueueFamilyIndex();
        ctx->m_context->UnlockCommandQueue();

        // Swapchain format
        auto& scDesc = ctx->m_swapchain->GetDesc();
        VkFormat format = DiligentToVkFormat(scDesc.ColorBufferFormat);

        // Render pass for ImGui pipeline
        if (!CreateImGuiRenderPass(vkDevice, format))
            return false;

        // Descriptor pool for ImGui
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100};
        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCI.maxSets = 100;
        poolCI.poolSizeCount = 1;
        poolCI.pPoolSizes = &poolSize;
        vkCreateDescriptorPool(vkDevice, &poolCI, nullptr, &g_sDescriptorPool);

        // ImGui setup
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ApplyForgeTheme();

        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo vkInit{};
        vkInit.Instance = vkInstance;
        vkInit.PhysicalDevice = vkPhysDev;
        vkInit.Device = vkDevice;
        vkInit.QueueFamily = queueFamily;
        vkInit.Queue = vkQueue;
        vkInit.DescriptorPool = g_sDescriptorPool;
        vkInit.MinImageCount = scDesc.BufferCount;
        vkInit.ImageCount = scDesc.BufferCount;
        vkInit.PipelineInfoMain.RenderPass = g_sRenderPass;
        vkInit.PipelineInfoMain.Subpass = 0;

        ImGui_ImplVulkan_Init(&vkInit);

        return true;
    }

    void ForgeImGuiShutdown(swarm::RenderContext* ctx) {
        vkDeviceWaitIdle(g_sDevice);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        for (auto& fb : g_sFramebuffers)
        {
            if (fb != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(g_sDevice, fb, nullptr);
                fb = VK_NULL_HANDLE;
            }
        }

        if (g_sRenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(g_sDevice, g_sRenderPass, nullptr);
            g_sRenderPass = VK_NULL_HANDLE;
        }

        if (g_sDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(g_sDevice, g_sDescriptorPool, nullptr);
            g_sDescriptorPool = VK_NULL_HANDLE;
        }

        g_sDevice = VK_NULL_HANDLE;
    }

    void ForgeImGuiNewFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ForgeImGuiRender(swarm::RenderContext* ctx) {
        ImGui::Render();

        auto& scDesc = ctx->m_swapchain->GetDesc();

        // Flush Diligent's pending commands (transitions + clear from BeginFrame)
        // so the backbuffer is in COLOR_ATTACHMENT_OPTIMAL before our raw Vulkan pass.
        ctx->m_context->Flush();

        // Get a fresh Vulkan command buffer for ImGui rendering
        auto* contextVk = static_cast<Diligent::IDeviceContextVk*>(ctx->m_context);
        VkCommandBuffer cmdBuf = contextVk->GetVkCommandBuffer();

        auto* pRTV = ctx->m_swapchain->GetCurrentBackBufferRTV();
        auto* rtvVk = static_cast<Diligent::ITextureViewVk*>(pRTV);
        VkImageView imageView = rtvVk->GetVulkanImageView();

        // Recycle old framebuffer, create new one for this frame's backbuffer
        if (g_sFramebuffers[g_sFrameIdx] != VK_NULL_HANDLE)
            vkDestroyFramebuffer(g_sDevice, g_sFramebuffers[g_sFrameIdx], nullptr);

        VkFramebufferCreateInfo fbCI{};
        fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCI.renderPass = g_sRenderPass;
        fbCI.attachmentCount = 1;
        fbCI.pAttachments = &imageView;
        fbCI.width = scDesc.Width;
        fbCI.height = scDesc.Height;
        fbCI.layers = 1;
        vkCreateFramebuffer(g_sDevice, &fbCI, nullptr, &g_sFramebuffers[g_sFrameIdx]);

        // Begin ImGui render pass
        VkRenderPassBeginInfo rpBI{};
        rpBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBI.renderPass = g_sRenderPass;
        rpBI.framebuffer = g_sFramebuffers[g_sFrameIdx];
        rpBI.renderArea = {{0, 0}, {scDesc.Width, scDesc.Height}};

        vkCmdBeginRenderPass(cmdBuf, &rpBI, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
        vkCmdEndRenderPass(cmdBuf);

        g_sFrameIdx = (g_sFrameIdx + 1) % kMaxFramesInFlight;
    }

    void* ForgeRegisterViewportRT(swarm::RenderContext* ctx, swarm::ViewportRT* rt) {
        auto* srv = static_cast<Diligent::ITextureView*>(swarm::GetViewportRTSRV(rt));
        auto* srvVk = static_cast<Diligent::ITextureViewVk*>(srv);
        VkImageView imageView = srvVk->GetVulkanImageView();

        VkSamplerCreateInfo samplerCI{};
        samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;

        VkSampler sampler = VK_NULL_HANDLE;
        vkCreateSampler(g_sDevice, &samplerCI, nullptr, &sampler);

        return ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void ForgeUnregisterViewportRT(void* textureId) {
        ImGui_ImplVulkan_RemoveTexture(static_cast<VkDescriptorSet>(textureId));
    }

} // namespace forge

#endif // HIVE_FEATURE_VULKAN
