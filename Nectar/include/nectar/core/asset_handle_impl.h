#pragma once

// This file provides the StrongHandle method bodies that need the full
// AssetServer definition. It is included at the bottom of asset_server.h.

namespace nectar
{
    template <typename T> StrongHandle<T>::~StrongHandle() noexcept {
        Release();
    }

    template <typename T>
    StrongHandle<T>::StrongHandle(const StrongHandle& other) noexcept
        : m_handle{other.m_handle}
        , m_server{other.m_server} {
        if (m_server && !m_handle.IsNull())
        {
            m_server->IncrementRef<T>(m_handle);
        }
    }

    template <typename T> StrongHandle<T>& StrongHandle<T>::operator=(const StrongHandle& other) noexcept {
        if (this != &other)
        {
            Release();
            m_handle = other.m_handle;
            m_server = other.m_server;
            if (m_server && !m_handle.IsNull())
            {
                m_server->IncrementRef<T>(m_handle);
            }
        }
        return *this;
    }

    template <typename T> void StrongHandle<T>::Release() noexcept {
        if (m_server && !m_handle.IsNull())
        {
            m_server->DecrementRef<T>(m_handle);
        }
        m_handle = wax::Handle<T>::Invalid();
        m_server = nullptr;
    }
} // namespace nectar
