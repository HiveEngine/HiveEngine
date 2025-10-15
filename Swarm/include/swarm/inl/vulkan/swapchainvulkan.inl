vkb::Swapchain m_Swapchain;
std::vector<VkImageView> m_SwapchainImageViews;

public:
    operator VkSwapchainKHR() { return m_Swapchain; }
    operator vkb::Swapchain() { return m_Swapchain; }
