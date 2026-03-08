#pragma once

#include <comb/allocator_concepts.h>
#include <comb/default_allocator.h>

#include <cstddef>
#include <type_traits>

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
        {
        }

        MemoryResource(const MemoryResource&) noexcept = default;
        MemoryResource(MemoryResource&&) noexcept = default;
        MemoryResource& operator=(const MemoryResource&) noexcept = default;
        MemoryResource& operator=(MemoryResource&&) noexcept = default;

        template <typename T>
            requires Allocator<T> && (!std::is_same_v<std::remove_cvref_t<T>, MemoryResource>)
        explicit MemoryResource(T& allocator) noexcept
            : m_object{&allocator}
            , m_allocate{&AllocateImpl<T>}
            , m_deallocate{&DeallocateImpl<T>}
            , m_getUsedMemory{&GetUsedMemoryImpl<T>}
            , m_getTotalMemory{&GetTotalMemoryImpl<T>}
            , m_getName{&GetNameImpl<T>}
        {
        }

        [[nodiscard]] void* Allocate(size_t size, size_t alignment) const
        {
            return m_allocate(m_object, size, alignment);
        }

        void Deallocate(void* ptr) const
        {
            m_deallocate(m_object, ptr);
        }

        [[nodiscard]] size_t GetUsedMemory() const
        {
            return m_getUsedMemory(m_object);
        }

        [[nodiscard]] size_t GetTotalMemory() const
        {
            return m_getTotalMemory(m_object);
        }

        [[nodiscard]] const char* GetName() const
        {
            return m_getName(m_object);
        }

    private:
        template <Allocator T> static void* AllocateImpl(void* object, size_t size, size_t alignment)
        {
            return static_cast<T*>(object)->Allocate(size, alignment);
        }

        template <Allocator T> static void DeallocateImpl(void* object, void* ptr)
        {
            static_cast<T*>(object)->Deallocate(ptr);
        }

        template <Allocator T> static size_t GetUsedMemoryImpl(void* object)
        {
            return static_cast<T*>(object)->GetUsedMemory();
        }

        template <Allocator T> static size_t GetTotalMemoryImpl(void* object)
        {
            return static_cast<T*>(object)->GetTotalMemory();
        }

        template <Allocator T> static const char* GetNameImpl(void* object)
        {
            return static_cast<T*>(object)->GetName();
        }

        void* m_object;
        AllocateFn m_allocate;
        DeallocateFn m_deallocate;
        GetMemoryFn m_getUsedMemory;
        GetMemoryFn m_getTotalMemory;
        GetNameFn m_getName;
    };

    [[nodiscard]] inline MemoryResource GetDefaultMemoryResource() noexcept
    {
        return MemoryResource{GetDefaultAllocator()};
    }
} // namespace comb