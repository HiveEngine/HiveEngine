#pragma once
namespace hive
{
    template <typename T> class Singleton
    {
    public:
        Singleton() {
            if (g_mInstance == nullptr)
            {
                g_mInstance = static_cast<T*>(this);
            }
        }

        ~Singleton() {
            if (g_mInstance == this)
            {
                g_mInstance = nullptr;
            }
        }

        static T& GetInstance() { return *g_mInstance; }

        [[nodiscard]] static inline bool IsInitialized() { return g_mInstance != nullptr; }

    protected:
        static inline T* g_mInstance = nullptr;
    };
} // namespace hive
