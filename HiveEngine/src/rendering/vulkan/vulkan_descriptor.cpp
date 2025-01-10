#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_descriptor.h"
#include "vulkan_types.h"

#include <core/Memory.h>
#include <core/Logger.h>

namespace hive::vk
{
    //Descriptor Set Layout Section
    //==============================
    VulkanDescriptorSetLayout::Builder::Builder(const VulkanDevice &device) : device_(device)
    {
    }

    VulkanDescriptorSetLayout::Builder & VulkanDescriptorSetLayout::Builder::addBinding(u32 binding_location,
        VkDescriptorType type, VkShaderStageFlags stage, u32 count)
    {
        VkDescriptorSetLayoutBinding layout_binding{};
        layout_binding.binding = binding_location;
        layout_binding.descriptorType = type;
        layout_binding.descriptorCount = count;
        layout_binding.stageFlags = stage;

        bindings_[binding_location] = layout_binding;

        return *this;
    }

    VulkanDescriptorSetLayout *VulkanDescriptorSetLayout::Builder::build() const
    {
        return Memory::createObject<VulkanDescriptorSetLayout, Memory::RENDERER>(device_, bindings_);
    }

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(const VulkanDevice &device,
        const std::unordered_map<u32, VkDescriptorSetLayoutBinding> &bindings) : device_(device)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings_vector;
        for(auto [location, layout] : bindings)
        {
            bindings_vector.push_back(layout);
        }

        VkDescriptorSetLayoutCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.bindingCount = static_cast<u32>(bindings.size());
        create_info.pBindings = bindings_vector.data();

        if(vkCreateDescriptorSetLayout(device_.logical_device, &create_info, nullptr, &layout_) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create descriptor set layout");
            return;
        }

        is_ready_ = true;
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        vkDestroyDescriptorSetLayout(device_.logical_device, layout_, nullptr);
    }

    VkDescriptorSetLayout& VulkanDescriptorSetLayout::getDescriptorSetLayout()
    {
        return layout_;
    }


    //Descriptor Pool Section
    //==============================
    VulkanDescriptorPool::Builder::Builder(const VulkanDevice &device) : device_(device), max_sets_(1000), pool_flags_(0)
    {

    }

    VulkanDescriptorPool::Builder & VulkanDescriptorPool::Builder::addPoolSize(VkDescriptorType type, u32 count)
    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = type;
        pool_size.descriptorCount = count;

        pool_sizes_.push_back(pool_size);

        return *this;
    }

    VulkanDescriptorPool::Builder & VulkanDescriptorPool::Builder::setPoolFlags(VkDescriptorPoolCreateFlags flags)
    {
        pool_flags_ = flags;
        return *this;
    }

    VulkanDescriptorPool::Builder & VulkanDescriptorPool::Builder::setMaxtSets(u32 count)
    {
        max_sets_ = count;
        return *this;
    }

    VulkanDescriptorPool* VulkanDescriptorPool::Builder::build() const
    {
        return Memory::createObject<VulkanDescriptorPool, Memory::RENDERER>(device_, max_sets_, pool_flags_, pool_sizes_);

    }


    VulkanDescriptorPool::VulkanDescriptorPool(const VulkanDevice &device, u32 maxSets,
                                               VkDescriptorPoolCreateFlags pool_flags, const std::vector<VkDescriptorPoolSize> &pool_sizes) : device_(device)
    {
        VkDescriptorPoolCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        create_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
        create_info.pPoolSizes = pool_sizes.data();
        create_info.flags = pool_flags;
        create_info.maxSets = maxSets;


        if(vkCreateDescriptorPool(device_.logical_device, &create_info, nullptr, &vk_descriptor_pool_) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create desciptor pool");
            return;
        }

        is_ready_ = true;
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        vkDestroyDescriptorPool(device_.logical_device, vk_descriptor_pool_, nullptr);
    }

    bool VulkanDescriptorPool::allocateDescriptor(const VkDescriptorSetLayout &descriptorSetLayout,
        VkDescriptorSet&descriptor) const
    {
        VkDescriptorSetAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = vk_descriptor_pool_;
        allocate_info.pSetLayouts = &descriptorSetLayout;
        allocate_info.descriptorSetCount = 1;

        if(vkAllocateDescriptorSets(device_.logical_device, &allocate_info, &descriptor) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to allocate descriptor set");
            return false;
        }
        return true;
    }
}
#endif
