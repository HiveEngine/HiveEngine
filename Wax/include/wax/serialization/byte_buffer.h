#pragma once

#include <hive/core/assert.h>

#include <comb/memory_resource.h>

#include <wax/containers/vector.h>
#include <wax/serialization/byte_span.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wax
{
    class ByteBuffer
    {
    public:
        ByteBuffer() noexcept
            : m_data{} {}

        explicit ByteBuffer(size_t initialCapacity) noexcept
            : m_data(initialCapacity) {}

        explicit ByteBuffer(comb::MemoryResource allocator) noexcept
            : m_data{allocator} {}

        template <comb::Allocator Allocator>
        explicit ByteBuffer(Allocator& allocator) noexcept
            : m_data{allocator} {}

        ByteBuffer(comb::MemoryResource allocator, size_t initialCapacity) noexcept
            : m_data{allocator, initialCapacity} {}

        template <comb::Allocator Allocator>
        ByteBuffer(Allocator& allocator, size_t initialCapacity) noexcept
            : m_data{allocator, initialCapacity} {}

        ByteBuffer(const ByteBuffer&) = delete;
        ByteBuffer& operator=(const ByteBuffer&) = delete;
        ByteBuffer(ByteBuffer&& other) noexcept = default;
        ByteBuffer& operator=(ByteBuffer&& other) noexcept = default;

        [[nodiscard]] const uint8_t* Data() const noexcept { return m_data.Data(); }
        [[nodiscard]] uint8_t* Data() noexcept { return m_data.Data(); }
        [[nodiscard]] size_t Size() const noexcept { return m_data.Size(); }
        [[nodiscard]] size_t Capacity() const noexcept { return m_data.Capacity(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return m_data.IsEmpty(); }
        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept { return m_data.GetAllocator(); }

        [[nodiscard]] ByteSpan View() const noexcept { return ByteSpan{m_data.Data(), m_data.Size()}; }

        [[nodiscard]] ByteSpan View(size_t offset, size_t count) const noexcept {
            hive::Assert(offset + count <= m_data.Size(), "ByteBuffer view out of bounds");
            return ByteSpan{m_data.Data() + offset, count};
        }

        void Reserve(size_t capacity) noexcept { m_data.Reserve(capacity); }
        void Resize(size_t newSize) noexcept { m_data.Resize(newSize); }
        void Clear() noexcept { m_data.Clear(); }

        void Append(const void* src, size_t size) noexcept {
            if (size == 0 || src == nullptr)
            {
                return;
            }

            const size_t oldSize = m_data.Size();
            m_data.Resize(oldSize + size);
            std::memcpy(m_data.Data() + oldSize, src, size);
        }

        void Append(ByteSpan view) noexcept { Append(view.Data(), view.Size()); }

        void Append(uint8_t byte) noexcept { m_data.PushBack(byte); }

        template <typename T> void Append(const T& value) noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            Append(&value, sizeof(T));
        }

        [[nodiscard]] uint8_t operator[](size_t index) const noexcept {
            hive::Assert(index < m_data.Size(), "ByteBuffer index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] uint8_t& operator[](size_t index) noexcept {
            hive::Assert(index < m_data.Size(), "ByteBuffer index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] const uint8_t* Begin() const noexcept { return m_data.Begin(); }
        [[nodiscard]] const uint8_t* End() const noexcept { return m_data.End(); }
        [[nodiscard]] uint8_t* Begin() noexcept { return m_data.Begin(); }
        [[nodiscard]] uint8_t* End() noexcept { return m_data.End(); }

    private:
        wax::Vector<uint8_t> m_data;
    };
} // namespace wax