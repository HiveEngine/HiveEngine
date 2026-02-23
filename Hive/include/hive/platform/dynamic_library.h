#pragma once

#include <cstddef>

namespace hive
{
    class DynamicLibrary
    {
    public:
        DynamicLibrary() = default;
        ~DynamicLibrary();

        DynamicLibrary(const DynamicLibrary&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;
        DynamicLibrary(DynamicLibrary&& other) noexcept;
        DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

        [[nodiscard]] bool Load(const char* path);
        void Unload();
        [[nodiscard]] void* GetSymbol(const char* name) const;

        template<typename Fn>
        [[nodiscard]] Fn GetFunction(const char* name) const
        {
            return reinterpret_cast<Fn>(GetSymbol(name));
        }

        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] const char* GetError() const noexcept;

    private:
        void* handle_{nullptr};
        static constexpr size_t kErrorBufSize = 256;
        mutable char error_buf_[kErrorBufSize]{};
    };
}
