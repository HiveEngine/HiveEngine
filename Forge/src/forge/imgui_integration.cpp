#include <forge/imgui_integration.h>
#include <forge/theme.h>

#include <swarm/imgui_bridge.h>
#include <swarm/swapchain.h>
#include <swarm/commands.h>
#include <swarm/device.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <ImGuizmo.h>

#if SWARM_VULKAN
#include <volk.h>
#include <imgui_impl_vulkan.h>
#endif

#if SWARM_D3D12
#include <imgui_impl_dx12.h>
#include <d3d12.h>
#endif

namespace forge
{
    // ========================================================================
    // Vulkan backend
    // ========================================================================

#if SWARM_VULKAN

    static VkDescriptorPool s_imgui_pool = VK_NULL_HANDLE;
    static VkSampler        s_linear_sampler = VK_NULL_HANDLE;

    bool ForgeImGuiInit(swarm::Device& device, swarm::Swapchain& swapchain, GLFWwindow* window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ApplyForgeTheme();

        ImGui_ImplGlfw_InitForVulkan(window, true);

        auto info = swarm::GetImGuiVulkanInfo(device);
        auto vk_device = static_cast<VkDevice>(info.device);

        // Descriptor pool for ImGui (separate from Swarm's bindless pool)
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
        };
        VkDescriptorPoolCreateInfo pool_ci{};
        pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_ci.maxSets = 100;
        pool_ci.poolSizeCount = 1;
        pool_ci.pPoolSizes = pool_sizes;
        vkCreateDescriptorPool(vk_device, &pool_ci, nullptr, &s_imgui_pool);

        // Dynamic rendering format
        auto sc_fmt = swarm::SwapchainGetFormat(swapchain);
        auto vk_format = static_cast<VkFormat>(swarm::GetNativeFormat(sc_fmt));

        ImGui_ImplVulkan_InitInfo vk_init{};
        vk_init.Instance = static_cast<VkInstance>(info.instance);
        vk_init.PhysicalDevice = static_cast<VkPhysicalDevice>(info.physical_device);
        vk_init.Device = vk_device;
        vk_init.Queue = static_cast<VkQueue>(info.graphics_queue);
        vk_init.QueueFamily = info.queue_family;
        vk_init.DescriptorPool = s_imgui_pool;
        vk_init.MinImageCount = info.image_count;
        vk_init.ImageCount = info.image_count;
        vk_init.UseDynamicRendering = true;
        vk_init.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        vk_init.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        vk_init.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vk_format;

        ImGui_ImplVulkan_Init(&vk_init);

        // Linear sampler for ImGui::Image (scene viewport)
        VkSamplerCreateInfo samp_ci{};
        samp_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samp_ci.magFilter = VK_FILTER_LINEAR;
        samp_ci.minFilter = VK_FILTER_LINEAR;
        samp_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samp_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samp_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samp_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(vk_device, &samp_ci, nullptr, &s_linear_sampler);

        return true;
    }

    void ForgeImGuiShutdown(swarm::Device& device)
    {
        auto info = swarm::GetImGuiVulkanInfo(device);
        auto vk_device = static_cast<VkDevice>(info.device);

        swarm::DeviceWaitIdle(device);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (s_linear_sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(vk_device, s_linear_sampler, nullptr);
            s_linear_sampler = VK_NULL_HANDLE;
        }

        if (s_imgui_pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(vk_device, s_imgui_pool, nullptr);
            s_imgui_pool = VK_NULL_HANDLE;
        }
    }

    void ForgeImGuiNewFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ForgeImGuiRender(swarm::CommandBuffer& cmd, swarm::Device& device,
                          swarm::TextureHandle backbuffer, uint32_t width, uint32_t height)
    {
        ImGui::Render();

        // Begin render pass for ImGui (Load existing scene content)
        swarm::ColorAttachment color_att{};
        color_att.texture = backbuffer;
        color_att.load_op = swarm::LoadOp::Load;
        color_att.store_op = swarm::StoreOp::Store;

        swarm::RenderingInfo ri{};
        ri.render_area = {0, 0, width, height};
        ri.color_attachments = &color_att;
        ri.color_attachment_count = 1;
        swarm::CmdBeginRendering(cmd, ri, device);

        auto* vk_cmd = static_cast<VkCommandBuffer>(swarm::GetRawCommandBuffer(cmd));
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_cmd);

        swarm::CmdEndRendering(cmd);
    }

    uint64_t ForgeRegisterTexture(swarm::Device& device, swarm::TextureHandle texture)
    {
        auto* view = static_cast<VkImageView>(swarm::GetTextureNativeView(device, texture));
        VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
            s_linear_sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return reinterpret_cast<uint64_t>(ds);
    }

    void ForgeUnregisterTexture(uint64_t texture_id)
    {
        if (texture_id)
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(texture_id));
    }

#endif // SWARM_VULKAN

    // ========================================================================
    // D3D12 backend
    // ========================================================================

#if SWARM_D3D12

    // SRV descriptor allocator for ImGui D3D12 backend.
    // Allocates from the tail of Swarm's SRV heap (slots 499999, 499998, ...).
    static uint32_t s_next_imgui_descriptor = 499999;
    static uint32_t s_srv_increment = 0;

    static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo* info,
                              D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu,
                              D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
    {
        uint32_t slot = s_next_imgui_descriptor--;

        D3D12_CPU_DESCRIPTOR_HANDLE cpu = info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpu = info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        cpu.ptr += static_cast<SIZE_T>(slot) * s_srv_increment;
        gpu.ptr += static_cast<UINT64>(slot) * s_srv_increment;

        *out_cpu = cpu;
        *out_gpu = gpu;
    }

    static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        // Slots reclaimed when the heap is destroyed
    }

    bool ForgeImGuiInit(swarm::Device& device, swarm::Swapchain& swapchain, GLFWwindow* window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ApplyForgeTheme();

        ImGui_ImplGlfw_InitForOther(window, true);

        auto swarm_info = swarm::GetImGuiD3D12Info(device);
        auto* heap = static_cast<ID3D12DescriptorHeap*>(swarm_info.cbv_srv_uav_heap);
        s_srv_increment = swarm_info.cbv_srv_uav_increment;

        auto sc_fmt = swarm::SwapchainGetFormat(swapchain);
        auto dx_format = static_cast<DXGI_FORMAT>(swarm::GetNativeFormat(sc_fmt));

        ImGui_ImplDX12_InitInfo init_info{};
        init_info.Device = static_cast<ID3D12Device*>(swarm_info.device);
        init_info.CommandQueue = static_cast<ID3D12CommandQueue*>(swarm_info.command_queue);
        init_info.NumFramesInFlight = static_cast<int>(swarm_info.num_frames_in_flight);
        init_info.RTVFormat = dx_format;
        init_info.SrvDescriptorHeap = heap;
        init_info.SrvDescriptorAllocFn = ImGuiSrvAlloc;
        init_info.SrvDescriptorFreeFn = ImGuiSrvFree;

        ImGui_ImplDX12_Init(&init_info);

        return true;
    }

    void ForgeImGuiShutdown(swarm::Device& device)
    {
        swarm::DeviceWaitIdle(device);
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ForgeImGuiNewFrame()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ForgeImGuiRender(swarm::CommandBuffer& cmd, swarm::Device& device,
                          swarm::TextureHandle backbuffer, uint32_t width, uint32_t height)
    {
        ImGui::Render();

        swarm::ColorAttachment color_att{};
        color_att.texture = backbuffer;
        color_att.load_op = swarm::LoadOp::Load;
        color_att.store_op = swarm::StoreOp::Store;

        swarm::RenderingInfo ri{};
        ri.render_area = {0, 0, width, height};
        ri.color_attachments = &color_att;
        ri.color_attachment_count = 1;
        swarm::CmdBeginRendering(cmd, ri, device);

        auto* dx_cmd = static_cast<ID3D12GraphicsCommandList*>(swarm::GetRawCommandBuffer(cmd));
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx_cmd);

        swarm::CmdEndRendering(cmd);
    }

    uint64_t ForgeRegisterTexture(swarm::Device& device, swarm::TextureHandle texture)
    {
        auto info = swarm::GetImGuiD3D12Info(device);
        auto* dx_device = static_cast<ID3D12Device*>(info.device);
        auto* heap = static_cast<ID3D12DescriptorHeap*>(info.cbv_srv_uav_heap);
        auto* resource = static_cast<ID3D12Resource*>(swarm::GetTextureNativeView(device, texture));
        if (!resource) return 0;

        uint32_t slot = s_next_imgui_descriptor--;

        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = heap->GetGPUDescriptorHandleForHeapStart();
        cpu_handle.ptr += static_cast<SIZE_T>(slot) * s_srv_increment;
        gpu_handle.ptr += static_cast<UINT64>(slot) * s_srv_increment;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        auto desc = resource->GetDesc();
        srv_desc.Format = desc.Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Texture2D.MipLevels = desc.MipLevels;
        dx_device->CreateShaderResourceView(resource, &srv_desc, cpu_handle);

        return gpu_handle.ptr;
    }

    void ForgeUnregisterTexture(uint64_t /*texture_id*/)
    {
        // D3D12 descriptors are freed when the heap is destroyed
    }

#endif // SWARM_D3D12
}
