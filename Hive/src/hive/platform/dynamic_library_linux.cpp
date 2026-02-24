#include <hive/platform/dynamic_library.h>

#include <dlfcn.h>
#include <cstring>

namespace hive
{

DynamicLibrary::~DynamicLibrary()
{
    Unload();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
    : handle_{other.handle_}
{
    std::memcpy(error_buf_, other.error_buf_, kErrorBufSize);
    other.handle_ = nullptr;
    other.error_buf_[0] = '\0';
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept
{
    if (this != &other)
    {
        Unload();
        handle_ = other.handle_;
        std::memcpy(error_buf_, other.error_buf_, kErrorBufSize);
        other.handle_ = nullptr;
        other.error_buf_[0] = '\0';
    }
    return *this;
}

bool DynamicLibrary::Load(const char* path)
{
    Unload();
    dlerror();
    handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle_)
    {
        const char* err = dlerror();
        if (err)
            std::strncpy(error_buf_, err, kErrorBufSize - 1);
        error_buf_[kErrorBufSize - 1] = '\0';
        return false;
    }
    error_buf_[0] = '\0';
    return true;
}

void DynamicLibrary::Unload()
{
    if (handle_)
    {
        dlclose(handle_);
        handle_ = nullptr;
    }
}

void* DynamicLibrary::GetSymbol(const char* name) const
{
    if (!handle_)
        return nullptr;
    dlerror();
    void* sym = dlsym(handle_, name);
    if (!sym)
    {
        const char* err = dlerror();
        if (err)
            std::strncpy(error_buf_, err, kErrorBufSize - 1);
        error_buf_[kErrorBufSize - 1] = '\0';
    }
    return sym;
}

bool DynamicLibrary::IsLoaded() const noexcept
{
    return handle_ != nullptr;
}

const char* DynamicLibrary::GetError() const noexcept
{
    return error_buf_;
}

} // namespace hive
