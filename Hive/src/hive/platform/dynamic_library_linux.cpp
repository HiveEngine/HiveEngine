#include <hive/platform/dynamic_library.h>

#include <cstring>
#include <dlfcn.h>

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
        dlerror();
        m_handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!m_handle)
        {
            const char* err = dlerror();
            if (err)
                std::strncpy(m_errorBuf, err, kErrorBufSize - 1);
            m_errorBuf[kErrorBufSize - 1] = '\0';
            return false;
        }
        m_errorBuf[0] = '\0';
        return true;
    }

    void DynamicLibrary::Unload()
    {
        if (m_handle)
        {
            dlclose(m_handle);
            m_handle = nullptr;
        }
    }

    void* DynamicLibrary::GetSymbol(const char* name) const
    {
        if (!m_handle)
            return nullptr;
        dlerror();
        void* sym = dlsym(m_handle, name);
        if (!sym)
        {
            const char* err = dlerror();
            if (err)
                std::strncpy(m_errorBuf, err, kErrorBufSize - 1);
            m_errorBuf[kErrorBufSize - 1] = '\0';
        }
        return sym;
    }

    void* DynamicLibrary::Detach() noexcept
    {
        void* handle = m_handle;
        m_handle = nullptr;
        m_errorBuf[0] = '\0';
        return handle;
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
