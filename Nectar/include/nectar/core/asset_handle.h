#pragma once

#include <wax/pointers/handle.h>
#include <cstdint>

namespace nectar
{
    class AssetServer;

    /// Non-owning handle to an asset. Does not participate in ref counting.
    /// 8 bytes, trivially copyable.
    template<typename T>
    struct WeakHandle
    {
        wax::Handle<T> raw{wax::Handle<T>::Invalid()};

        [[nodiscard]] constexpr bool IsNull() const noexcept { return raw.IsNull(); }

        [[nodiscard]] constexpr bool operator==(const WeakHandle& other) const noexcept
        {
            return raw == other.raw;
        }

        [[nodiscard]] constexpr bool operator!=(const WeakHandle& other) const noexcept
        {
            return !(raw == other.raw);
        }

        [[nodiscard]] static constexpr WeakHandle Invalid() noexcept { return WeakHandle{}; }
    };

    /// Owning handle to an asset. RAII ref counting — copies increment,
    /// destruction decrements. 16 bytes (handle + server pointer).
    template<typename T>
    class StrongHandle
    {
    public:
        constexpr StrongHandle() noexcept
            : handle_{wax::Handle<T>::Invalid()}
            , server_{nullptr}
        {}

        ~StrongHandle() noexcept;

        StrongHandle(const StrongHandle& other) noexcept;
        StrongHandle& operator=(const StrongHandle& other) noexcept;

        StrongHandle(StrongHandle&& other) noexcept
            : handle_{other.handle_}
            , server_{other.server_}
        {
            other.handle_ = wax::Handle<T>::Invalid();
            other.server_ = nullptr;
        }

        StrongHandle& operator=(StrongHandle&& other) noexcept
        {
            if (this != &other)
            {
                Release();
                handle_ = other.handle_;
                server_ = other.server_;
                other.handle_ = wax::Handle<T>::Invalid();
                other.server_ = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsNull() const noexcept { return handle_.IsNull(); }

        [[nodiscard]] WeakHandle<T> MakeWeak() const noexcept
        {
            return WeakHandle<T>{handle_};
        }

        [[nodiscard]] wax::Handle<T> Raw() const noexcept { return handle_; }

        [[nodiscard]] bool operator==(const StrongHandle& other) const noexcept
        {
            return handle_ == other.handle_;
        }

        [[nodiscard]] bool operator!=(const StrongHandle& other) const noexcept
        {
            return !(handle_ == other.handle_);
        }

    private:
        friend class AssetServer;

        StrongHandle(wax::Handle<T> handle, AssetServer* server) noexcept
            : handle_{handle}
            , server_{server}
        {}

        void Release() noexcept;

        wax::Handle<T> handle_;
        AssetServer* server_;
    };
}
