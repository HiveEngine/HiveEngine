#pragma once
namespace hive
{
    template<typename T>
    class Singleton
    {
    public:
        Singleton()
        {
            if (m_Instance == nullptr)
            {
                m_Instance = static_cast<T*>(this);
            }
        }

        ~Singleton()
        {
            if (m_Instance == this)
            {
                m_Instance = nullptr;
            }
        }

        static T& GetInstance()
        {
            return *m_Instance;
        }

        [[nodiscard]] static inline bool IsInitialized()
        {
            return m_Instance != nullptr;
        }

    protected:
        static inline T* m_Instance = nullptr;
    };
}