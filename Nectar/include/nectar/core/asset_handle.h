#pragma once

#include <wax/pointers/handle.h>

#include <cstdint>

namespace nectar
{
    class AssetServer;

    /// Non-owning handle to an asset. Does not participate in ref counting.
    /// 8 bytes, trivially copyable.
    template <typename T> struct WeakHandle
    {
        wax::Handle<T> m_raw{wax::Handle<T>::Invalid()};

        [[nodiscard]] constexpr bool IsNull() const noexcept
        {
            return m_raw.IsNull();
        }

        [[nodiscard]] constexpr bool operator==(const WeakHandle& other) const noexcept
        {
            return m_raw == other.m_raw;
        }

        [[nodiscard]] constexpr bool operator!=(const WeakHandle& other) const noexcept
        {
            return !(m_raw == other.m_raw);
        }

        [[nodiscard]] static constexpr WeakHandle Invalid() noexcept
        {
            return WeakHandle{};
        }
    };

    /// Owning handle to an asset. RAII ref counting — copies increment,
    /// destruction decrements. 16 bytes (handle + server pointer).
    template <typename T> class StrongHandle
    {
    public:
        constexpr StrongHandle() noexcept
            : m_handle{wax::Handle<T>::Invalid()}
            , m_server{nullptr}
        {
        }

        ~StrongHandle() noexcept;

        StrongHandle(const StrongHandle& other) noexcept;
        StrongHandle& operator=(const StrongHandle& other) noexcept;

        StrongHandle(StrongHandle&& other) noexcept
            : m_handle{other.m_handle}
            , m_server{other.m_server}
        {
            other.m_handle = wax::Handle<T>::Invalid();
            other.m_server = nullptr;
        }

        StrongHandle& operator=(StrongHandle&& other) noexcept
        {
            if (this != &other)
            {
                Release();
                m_handle = other.m_handle;
                m_server = other.m_server;
                other.m_handle = wax::Handle<T>::Invalid();
                other.m_server = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsNull() const noexcept
        {
            return m_handle.IsNull();
        }

        [[nodiscard]] WeakHandle<T> MakeWeak() const noexcept
        {
            return WeakHandle<T>{m_handle};
        }

        [[nodiscard]] wax::Handle<T> Raw() const noexcept
        {
            return m_handle;
        }

        [[nodiscard]] bool operator==(const StrongHandle& other) const noexcept
        {
            return m_handle == other.m_handle;
        }

        [[nodiscard]] bool operator!=(const StrongHandle& other) const noexcept
        {
            return !(m_handle == other.m_handle);
        }

    private:
        friend class AssetServer;

        StrongHandle(wax::Handle<T> handle, AssetServer* server) noexcept
            : m_handle{handle}
            , m_server{server}
        {
        }

        void Release() noexcept;

        wax::Handle<T> m_handle;
        AssetServer* m_server;
    };
} // namespace nectar
