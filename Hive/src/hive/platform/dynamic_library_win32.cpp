#include <hive/platform/dynamic_library.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>

namespace hive
{

    DynamicLibrary::~DynamicLibrary()
    {
        Unload();
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
        : m_handle{other.m_handle}
    {
        std::memcpy(m_errorBuf, other.m_errorBuf, kErrorBufSize);
        other.m_handle = nullptr;
        other.m_errorBuf[0] = '\0';
    }

    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept
    {
        if (this != &other)
        {
            Unload();
            m_handle = other.m_handle;
            std::memcpy(m_errorBuf, other.m_errorBuf, kErrorBufSize);
            other.m_handle = nullptr;
            other.m_errorBuf[0] = '\0';
        }
        return *this;
    }

    bool DynamicLibrary::Load(const char* path)
    {
        Unload();
        m_handle = static_cast<void*>(LoadLibraryA(path));
        if (!m_handle)
        {
            DWORD err = GetLastError();
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, m_errorBuf,
                           static_cast<DWORD>(kErrorBufSize), nullptr);
            size_t len = std::strlen(m_errorBuf);
            while (len > 0 && (m_errorBuf[len - 1] == '\n' || m_errorBuf[len - 1] == '\r'))
                m_errorBuf[--len] = '\0';
            return false;
        }
        m_errorBuf[0] = '\0';
        return true;
    }

    void DynamicLibrary::Unload()
    {
        if (m_handle)
        {
            FreeLibrary(static_cast<HMODULE>(m_handle));
            m_handle = nullptr;
        }
    }

    void* DynamicLibrary::GetSymbol(const char* name) const
    {
        if (!m_handle)
            return nullptr;
        void* sym = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
        if (!sym)
        {
            DWORD err = GetLastError();
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, m_errorBuf,
                           static_cast<DWORD>(kErrorBufSize), nullptr);
            size_t len = std::strlen(m_errorBuf);
            while (len > 0 && (m_errorBuf[len - 1] == '\n' || m_errorBuf[len - 1] == '\r'))
                m_errorBuf[--len] = '\0';
        }
        return sym;
    }

    bool DynamicLibrary::IsLoaded() const noexcept
    {
        return m_handle != nullptr;
    }

    const char* DynamicLibrary::GetError() const noexcept
    {
        return m_errorBuf;
    }

} // namespace hive
