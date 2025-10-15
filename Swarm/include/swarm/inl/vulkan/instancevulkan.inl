vkb::Instance m_Instance;

public:
    operator VkInstance() const { return m_Instance; }
    operator vkb::Instance() const { return m_Instance; }
