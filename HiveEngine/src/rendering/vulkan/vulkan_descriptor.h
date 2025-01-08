#pragma once
#include <hvpch.h>
#include <unordered_map>
#include "vulkan_types.h"

#include <vulkan/vulkan.h>
namespace hive::vk
{
    // struct VulkanDevice;

    class VulkanDescriptorSetLayout
    {
    public:
        class Builder
        {
        public:
            explicit Builder(const VulkanDevice& device);
            Builder &addBinding(u32 binding_location, VkDescriptorType type, VkShaderStageFlags stage, u32 count = 1);
            VulkanDescriptorSetLayout *build() const;

        private:
            const VulkanDevice &device_;

            std::unordered_map<u32, VkDescriptorSetLayoutBinding> bindings_{};
        };

        VulkanDescriptorSetLayout(const VulkanDevice& device, const std::unordered_map<u32, VkDescriptorSetLayoutBinding> &bindings);
        ~VulkanDescriptorSetLayout();
        VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout& ) = delete;
        VulkanDescriptorSetLayout &operator=(const VulkanDescriptorSetLayout&) = delete;
        [[nodiscard]] VkDescriptorSetLayout &getDescriptorSetLayout();

        [[nodiscard]] bool isReady() const { return is_ready_; }

    private:
        VkDescriptorSetLayout layout_{};
        const VulkanDevice& device_;
        bool is_ready_ = false;;
    };

    class VulkanDescriptorPool
    {
    public:
        class Builder
        {
        public:
            explicit Builder(const VulkanDevice& device);

            Builder &addPoolSize(VkDescriptorType type, u32 count);
            Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
            Builder &setMaxtSets(u32 count);

            [[nodiscard]] VulkanDescriptorPool* build() const;

        private:
            const VulkanDevice &device_;

            std::vector<VkDescriptorPoolSize> pool_sizes_;
            VkDescriptorPoolCreateFlags pool_flags_;
            u32 max_sets_;
        };

        VulkanDescriptorPool(const VulkanDevice& device, u32 maxSets, VkDescriptorPoolCreateFlags pool_flags, const std::vector<VkDescriptorPoolSize> &pool_sizes);
        ~VulkanDescriptorPool();

        VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;

        [[nodiscard]] bool isReady() const { return is_ready_; }

        [[nodiscard]] VkDescriptorPool &getDescriptorPool() { return vk_descriptor_pool_; }

        bool allocateDescriptor(const VkDescriptorSetLayout &descriptorSetLayout, VkDescriptorSet &descriptor) const;


    private:
        bool is_ready_ = false;
        const VulkanDevice& device_;
        VkDescriptorPool vk_descriptor_pool_{};
    };
}