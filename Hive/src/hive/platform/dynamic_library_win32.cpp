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
    handle_ = static_cast<void*>(LoadLibraryA(path));
    if (!handle_)
    {
        DWORD err = GetLastError();
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, 0,
            error_buf_, static_cast<DWORD>(kErrorBufSize), nullptr);
        size_t len = std::strlen(error_buf_);
        while (len > 0 && (error_buf_[len - 1] == '\n' || error_buf_[len - 1] == '\r'))
            error_buf_[--len] = '\0';
        return false;
    }
    error_buf_[0] = '\0';
    return true;
}

void DynamicLibrary::Unload()
{
    if (handle_)
    {
        FreeLibrary(static_cast<HMODULE>(handle_));
        handle_ = nullptr;
    }
}

void* DynamicLibrary::GetSymbol(const char* name) const
{
    if (!handle_)
        return nullptr;
    void* sym = reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle_), name));
    if (!sym)
    {
        DWORD err = GetLastError();
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, 0,
            error_buf_, static_cast<DWORD>(kErrorBufSize), nullptr);
        size_t len = std::strlen(error_buf_);
        while (len > 0 && (error_buf_[len - 1] == '\n' || error_buf_[len - 1] == '\r'))
            error_buf_[--len] = '\0';
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
