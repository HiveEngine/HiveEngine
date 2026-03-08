#pragma once

#include <cstddef>
#include <type_traits>
#include <comb/allocator_concepts.h>
#include <comb/default_allocator.h>

namespace comb
{
    class MemoryResource
    {
    public:
        using AllocateFn = void* (*)(void* object, size_t size, size_t alignment);
        using DeallocateFn = void (*)(void* object, void* ptr);
        using GetMemoryFn = size_t (*)(void* object);
        using GetNameFn = const char* (*)(void* object);

        MemoryResource() noexcept
            : MemoryResource{GetDefaultAllocator()}
        {}

        MemoryResource(const MemoryResource&) noexcept = default;
        MemoryResource(MemoryResource&&) noexcept = default;
        MemoryResource& operator=(const MemoryResource&) noexcept = default;
        MemoryResource& operator=(MemoryResource&&) noexcept = default;

        template<typename T>
            requires Allocator<T> && (!std::is_same_v<std::remove_cvref_t<T>, MemoryResource>)
        explicit MemoryResource(T& allocator) noexcept
            : object_{&allocator}
            , allocate_{&AllocateImpl<T>}
            , deallocate_{&DeallocateImpl<T>}
            , get_used_memory_{&GetUsedMemoryImpl<T>}
            , get_total_memory_{&GetTotalMemoryImpl<T>}
            , get_name_{&GetNameImpl<T>}
        {}

        [[nodiscard]] void* Allocate(size_t size, size_t alignment) const
        {
            return allocate_(object_, size, alignment);
        }

        void Deallocate(void* ptr) const
        {
            deallocate_(object_, ptr);
        }

        [[nodiscard]] size_t GetUsedMemory() const
        {
            return get_used_memory_(object_);
        }

        [[nodiscard]] size_t GetTotalMemory() const
        {
            return get_total_memory_(object_);
        }

        [[nodiscard]] const char* GetName() const
        {
            return get_name_(object_);
        }

    private:
        template<Allocator T>
        static void* AllocateImpl(void* object, size_t size, size_t alignment)
        {
            return static_cast<T*>(object)->Allocate(size, alignment);
        }

        template<Allocator T>
        static void DeallocateImpl(void* object, void* ptr)
        {
            static_cast<T*>(object)->Deallocate(ptr);
        }

        template<Allocator T>
        static size_t GetUsedMemoryImpl(void* object)
        {
            return static_cast<T*>(object)->GetUsedMemory();
        }

        template<Allocator T>
        static size_t GetTotalMemoryImpl(void* object)
        {
            return static_cast<T*>(object)->GetTotalMemory();
        }

        template<Allocator T>
        static const char* GetNameImpl(void* object)
        {
            return static_cast<T*>(object)->GetName();
        }

        void* object_;
        AllocateFn allocate_;
        DeallocateFn deallocate_;
        GetMemoryFn get_used_memory_;
        GetMemoryFn get_total_memory_;
        GetNameFn get_name_;
    };

    [[nodiscard]] inline MemoryResource GetDefaultMemoryResource() noexcept
    {
        return MemoryResource{GetDefaultAllocator()};
    }
}