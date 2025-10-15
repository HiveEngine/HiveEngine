vkb::PhysicalDevice m_PhysicalDevice;
vkb::Device m_Device;

public:
    operator VkDevice() const { return m_Device; }
    operator vkb::Device() const { return m_Device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }