#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace queen
{
    /**
     * Entity identifier with generation counter
     *
     * 64-bit packed structure containing:
     * - Index (32 bits): Slot in entity storage, allows ~4 billion entities
     * - Generation (16 bits): Incremented on recycle, detects use-after-free
     * - Flags (16 bits): Entity state flags (disabled, pending delete, etc.)
     *
     * Memory layout (64 bits total):
     * ┌────────────────────────────────────────────────────────────────┐
     * │ Bits 0-31:  Index (entity slot)                                │
     * │ Bits 32-47: Generation (use-after-free detection)              │
     * │ Bits 48-63: Flags                                              │
     * │   Bit 48:   Alive flag                                         │
     * │   Bit 49:   Disabled flag                                      │
     * │   Bit 50:   Pending delete flag                                │
     * │   Bit 51:   Has relationships flag                             │
     * │   Bits 52-63: Reserved                                         │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Size: 8 bytes (fits in register)
     * - Comparison: O(1) - single 64-bit compare
     * - Hash: O(1) - identity hash or FNV-1a
     * - Copy: Trivial (memcpy-able)
     *
     * Limitations:
     * - Max ~4 billion concurrent entities (32-bit index)
     * - Generation wraps after 65536 recycles (false positives rare)
     * - Entity validity requires EntityAllocator lookup
     *
     * Example:
     * @code
     *   Entity e = allocator.Allocate();
     *   uint32_t idx = e.Index();
     *   uint16_t gen = e.Generation();
     *
     *   allocator.Deallocate(e);
     *   // e.Index() still valid, but IsAlive(e) returns false
     * @endcode
     */
    class Entity
    {
    public:
        using IndexType = uint32_t;
        using GenerationType = uint16_t;
        using FlagsType = uint16_t;

        static constexpr IndexType kMaxIndex = UINT32_MAX - 1;
        static constexpr GenerationType kMaxGeneration = UINT16_MAX;

        struct Flags
        {
            static constexpr FlagsType kNone = 0;
            static constexpr FlagsType kAlive = 1 << 0;
            static constexpr FlagsType kDisabled = 1 << 1;
            static constexpr FlagsType kPendingDelete = 1 << 2;
            static constexpr FlagsType kHasRelationships = 1 << 3;
        };

        constexpr Entity() noexcept
            : index_{UINT32_MAX}
            , generation_{0}
            , flags_{0}
        {}

        constexpr Entity(IndexType index, GenerationType generation, FlagsType flags = Flags::kAlive) noexcept
            : index_{index}
            , generation_{generation}
            , flags_{flags}
        {}

        [[nodiscard]] static constexpr Entity Invalid() noexcept
        {
            return Entity{};
        }

        [[nodiscard]] constexpr IndexType Index() const noexcept
        {
            return index_;
        }

        [[nodiscard]] constexpr GenerationType Generation() const noexcept
        {
            return generation_;
        }

        [[nodiscard]] constexpr FlagsType GetFlags() const noexcept
        {
            return flags_;
        }

        [[nodiscard]] constexpr bool IsNull() const noexcept
        {
            return index_ == UINT32_MAX;
        }

        [[nodiscard]] constexpr bool HasFlag(FlagsType flag) const noexcept
        {
            return (flags_ & flag) != 0;
        }

        [[nodiscard]] constexpr bool IsAlive() const noexcept
        {
            return HasFlag(Flags::kAlive);
        }

        [[nodiscard]] constexpr bool IsDisabled() const noexcept
        {
            return HasFlag(Flags::kDisabled);
        }

        [[nodiscard]] constexpr bool IsPendingDelete() const noexcept
        {
            return HasFlag(Flags::kPendingDelete);
        }

        constexpr void SetFlag(FlagsType flag) noexcept
        {
            flags_ |= flag;
        }

        constexpr void ClearFlag(FlagsType flag) noexcept
        {
            flags_ &= static_cast<FlagsType>(~flag);
        }

        [[nodiscard]] constexpr bool operator==(const Entity& other) const noexcept
        {
            return index_ == other.index_ && generation_ == other.generation_;
        }

        [[nodiscard]] constexpr bool operator!=(const Entity& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator<(const Entity& other) const noexcept
        {
            if (index_ != other.index_)
            {
                return index_ < other.index_;
            }
            return generation_ < other.generation_;
        }

        [[nodiscard]] constexpr uint64_t ToU64() const noexcept
        {
            return static_cast<uint64_t>(index_)
                 | (static_cast<uint64_t>(generation_) << 32)
                 | (static_cast<uint64_t>(flags_) << 48);
        }

        [[nodiscard]] static constexpr Entity FromU64(uint64_t value) noexcept
        {
            return Entity{
                static_cast<IndexType>(value & 0xFFFFFFFF),
                static_cast<GenerationType>((value >> 32) & 0xFFFF),
                static_cast<FlagsType>((value >> 48) & 0xFFFF)
            };
        }

    private:
        IndexType index_;
        GenerationType generation_;
        FlagsType flags_;
    };

    static_assert(sizeof(Entity) == 8, "Entity must be 8 bytes");
    static_assert(std::is_trivially_copyable_v<Entity>, "Entity must be trivially copyable");
}

namespace std
{
    template<>
    struct hash<queen::Entity>
    {
        size_t operator()(const queen::Entity& e) const noexcept
        {
            return e.ToU64();
        }
    };
}
