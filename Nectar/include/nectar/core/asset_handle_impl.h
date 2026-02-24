#pragma once

// This file provides the StrongHandle method bodies that need the full
// AssetServer definition. It is included at the bottom of asset_server.h.

namespace nectar
{
    template<typename T>
    StrongHandle<T>::~StrongHandle() noexcept
    {
        Release();
    }

    template<typename T>
    StrongHandle<T>::StrongHandle(const StrongHandle& other) noexcept
        : handle_{other.handle_}
        , server_{other.server_}
    {
        if (server_ && !handle_.IsNull())
        {
            server_->IncrementRef<T>(handle_);
        }
    }

    template<typename T>
    StrongHandle<T>& StrongHandle<T>::operator=(const StrongHandle& other) noexcept
    {
        if (this != &other)
        {
            Release();
            handle_ = other.handle_;
            server_ = other.server_;
            if (server_ && !handle_.IsNull())
            {
                server_->IncrementRef<T>(handle_);
            }
        }
        return *this;
    }

    template<typename T>
    void StrongHandle<T>::Release() noexcept
    {
        if (server_ && !handle_.IsNull())
        {
            server_->DecrementRef<T>(handle_);
        }
        handle_ = wax::Handle<T>::Invalid();
        server_ = nullptr;
    }
}
