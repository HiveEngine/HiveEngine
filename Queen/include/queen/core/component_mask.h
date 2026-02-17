#pragma once

#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>
#include <cstdint>

namespace queen
{
    /**
     * Dynamic bitset for tracking component presence
     *
     * ComponentMask provides O(1) set/clear/test operations and O(n/64) logical
     * operations where n is the highest bit index. Used by AccessDescriptor for
     * fast conflict detection between systems.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ blocks_: Vector<uint64_t>                                  │
     * │   [block0: bits 0-63] [block1: bits 64-127] ...            │
     * └────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Set/Clear/Test: O(1)
     * - And/Or/Xor: O(n/64) where n = max bit index
     * - Any/None/Count: O(n/64)
     * - Memory: 8 bytes per 64 components
     *
     * Use cases:
     * - Tracking component reads/writes for parallel scheduling
     * - Archetype matching (which components are present)
     * - Fast intersection tests for query matching
     *
     * Limitations:
     * - Grows dynamically (may allocate)
     * - Not thread-safe
     *
     * Example:
     * @code
     *   ComponentMask mask{alloc};
     *   mask.Set(TypeIdOf<Position>() % 1024);  // Use modulo for index
     *   mask.Set(TypeIdOf<Velocity>() % 1024);
     *
     *   if (mask.Test(TypeIdOf<Position>() % 1024)) {
     *       // Has Position
     *   }
     *
     *   // Check for overlap with another mask
     *   if (mask.Intersects(other_mask)) {
     *       // Conflict!
     *   }
     * @endcode
     */
    template<comb::Allocator Allocator>
    class ComponentMask
    {
    public:
        static constexpr size_t BitsPerBlock = 64;

        explicit ComponentMask(Allocator& allocator)
            : allocator_{&allocator}
            , blocks_{allocator}
        {
        }

        /**
         * Copy constructor - uses the same allocator as other
         */
        ComponentMask(const ComponentMask& other)
            : allocator_{other.allocator_}
            , blocks_{*other.allocator_}
        {
            blocks_.Reserve(other.blocks_.Size());
            for (size_t i = 0; i < other.blocks_.Size(); ++i)
            {
                blocks_.PushBack(other.blocks_[i]);
            }
        }

        /**
         * Copy assignment - uses own allocator
         */
        ComponentMask& operator=(const ComponentMask& other)
        {
            if (this != &other)
            {
                blocks_.Clear();
                blocks_.Reserve(other.blocks_.Size());
                for (size_t i = 0; i < other.blocks_.Size(); ++i)
                {
                    blocks_.PushBack(other.blocks_[i]);
                }
            }
            return *this;
        }

        ComponentMask(ComponentMask&& other) noexcept
            : allocator_{other.allocator_}
            , blocks_{std::move(other.blocks_)}
        {
        }

        ComponentMask& operator=(ComponentMask&& other) noexcept
        {
            if (this != &other)
            {
                allocator_ = other.allocator_;
                blocks_ = std::move(other.blocks_);
            }
            return *this;
        }

        /**
         * Set a bit at the given index
         */
        void Set(size_t index)
        {
            size_t block_index = index / BitsPerBlock;
            size_t bit_index = index % BitsPerBlock;

            EnsureCapacity(block_index + 1);
            blocks_[block_index] |= (uint64_t{1} << bit_index);
        }

        /**
         * Clear a bit at the given index
         */
        void Clear(size_t index)
        {
            size_t block_index = index / BitsPerBlock;
            if (block_index >= blocks_.Size())
            {
                return;
            }

            size_t bit_index = index % BitsPerBlock;
            blocks_[block_index] &= ~(uint64_t{1} << bit_index);
        }

        /**
         * Test if a bit is set
         */
        [[nodiscard]] bool Test(size_t index) const noexcept
        {
            size_t block_index = index / BitsPerBlock;
            if (block_index >= blocks_.Size())
            {
                return false;
            }

            size_t bit_index = index % BitsPerBlock;
            return (blocks_[block_index] & (uint64_t{1} << bit_index)) != 0;
        }

        /**
         * Toggle a bit at the given index
         */
        void Toggle(size_t index)
        {
            size_t block_index = index / BitsPerBlock;
            size_t bit_index = index % BitsPerBlock;

            EnsureCapacity(block_index + 1);
            blocks_[block_index] ^= (uint64_t{1} << bit_index);
        }

        /**
         * Clear all bits
         */
        void ClearAll()
        {
            for (size_t i = 0; i < blocks_.Size(); ++i)
            {
                blocks_[i] = 0;
            }
        }

        /**
         * Set all bits up to count
         */
        void SetAll(size_t count)
        {
            size_t block_count = (count + BitsPerBlock - 1) / BitsPerBlock;
            EnsureCapacity(block_count);

            for (size_t i = 0; i < block_count; ++i)
            {
                if (i < block_count - 1)
                {
                    blocks_[i] = ~uint64_t{0};
                }
                else
                {
                    size_t remaining_bits = count % BitsPerBlock;
                    if (remaining_bits == 0)
                    {
                        blocks_[i] = ~uint64_t{0};
                    }
                    else
                    {
                        blocks_[i] = (uint64_t{1} << remaining_bits) - 1;
                    }
                }
            }
        }

        /**
         * Check if any bit is set
         */
        [[nodiscard]] bool Any() const noexcept
        {
            for (size_t i = 0; i < blocks_.Size(); ++i)
            {
                if (blocks_[i] != 0)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * Check if no bits are set
         */
        [[nodiscard]] bool None() const noexcept
        {
            return !Any();
        }

        /**
         * Count the number of set bits
         */
        [[nodiscard]] size_t Count() const noexcept
        {
            size_t count = 0;
            for (size_t i = 0; i < blocks_.Size(); ++i)
            {
                count += PopCount(blocks_[i]);
            }
            return count;
        }

        /**
         * Check if this mask has any overlap with another
         */
        [[nodiscard]] bool Intersects(const ComponentMask& other) const noexcept
        {
            size_t min_size = blocks_.Size() < other.blocks_.Size()
                ? blocks_.Size()
                : other.blocks_.Size();

            for (size_t i = 0; i < min_size; ++i)
            {
                if ((blocks_[i] & other.blocks_[i]) != 0)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * Check if this mask contains all bits from another
         */
        [[nodiscard]] bool ContainsAll(const ComponentMask& other) const noexcept
        {
            for (size_t i = 0; i < other.blocks_.Size(); ++i)
            {
                uint64_t our_block = (i < blocks_.Size()) ? blocks_[i] : 0;
                if ((our_block & other.blocks_[i]) != other.blocks_[i])
                {
                    return false;
                }
            }
            return true;
        }

        /**
         * Check if this mask has no overlap with another
         */
        [[nodiscard]] bool Disjoint(const ComponentMask& other) const noexcept
        {
            return !Intersects(other);
        }

        /**
         * Bitwise AND with another mask (intersection)
         */
        ComponentMask& operator&=(const ComponentMask& other)
        {
            size_t min_size = blocks_.Size() < other.blocks_.Size()
                ? blocks_.Size()
                : other.blocks_.Size();

            for (size_t i = 0; i < min_size; ++i)
            {
                blocks_[i] &= other.blocks_[i];
            }

            // Clear bits beyond other's size
            for (size_t i = min_size; i < blocks_.Size(); ++i)
            {
                blocks_[i] = 0;
            }

            return *this;
        }

        /**
         * Bitwise OR with another mask (union)
         */
        ComponentMask& operator|=(const ComponentMask& other)
        {
            EnsureCapacity(other.blocks_.Size());

            for (size_t i = 0; i < other.blocks_.Size(); ++i)
            {
                blocks_[i] |= other.blocks_[i];
            }

            return *this;
        }

        /**
         * Bitwise XOR with another mask (symmetric difference)
         */
        ComponentMask& operator^=(const ComponentMask& other)
        {
            EnsureCapacity(other.blocks_.Size());

            for (size_t i = 0; i < other.blocks_.Size(); ++i)
            {
                blocks_[i] ^= other.blocks_[i];
            }

            return *this;
        }

        /**
         * Bitwise NOT (complement)
         */
        void Invert()
        {
            for (size_t i = 0; i < blocks_.Size(); ++i)
            {
                blocks_[i] = ~blocks_[i];
            }
        }

        /**
         * Equality comparison
         */
        [[nodiscard]] bool operator==(const ComponentMask& other) const noexcept
        {
            size_t max_size = blocks_.Size() > other.blocks_.Size()
                ? blocks_.Size()
                : other.blocks_.Size();

            for (size_t i = 0; i < max_size; ++i)
            {
                uint64_t our_block = (i < blocks_.Size()) ? blocks_[i] : 0;
                uint64_t their_block = (i < other.blocks_.Size()) ? other.blocks_[i] : 0;

                if (our_block != their_block)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool operator!=(const ComponentMask& other) const noexcept
        {
            return !(*this == other);
        }

        /**
         * Get the index of the first set bit, or size_t(-1) if none
         */
        [[nodiscard]] size_t FirstSetBit() const noexcept
        {
            for (size_t i = 0; i < blocks_.Size(); ++i)
            {
                if (blocks_[i] != 0)
                {
                    return i * BitsPerBlock + CountTrailingZeros(blocks_[i]);
                }
            }
            return static_cast<size_t>(-1);
        }

        /**
         * Get the index of the last set bit, or size_t(-1) if none
         */
        [[nodiscard]] size_t LastSetBit() const noexcept
        {
            for (size_t i = blocks_.Size(); i > 0; --i)
            {
                if (blocks_[i - 1] != 0)
                {
                    return (i - 1) * BitsPerBlock + (BitsPerBlock - 1 - CountLeadingZeros(blocks_[i - 1]));
                }
            }
            return static_cast<size_t>(-1);
        }

        /**
         * Get the number of blocks allocated
         */
        [[nodiscard]] size_t BlockCount() const noexcept
        {
            return blocks_.Size();
        }

        /**
         * Get the maximum bit index that can be stored without reallocation
         */
        [[nodiscard]] size_t Capacity() const noexcept
        {
            return blocks_.Size() * BitsPerBlock;
        }

        /**
         * Reserve space for at least n bits
         */
        void Reserve(size_t bit_count)
        {
            size_t block_count = (bit_count + BitsPerBlock - 1) / BitsPerBlock;
            blocks_.Reserve(block_count);
        }

    private:
        void EnsureCapacity(size_t block_count)
        {
            while (blocks_.Size() < block_count)
            {
                blocks_.PushBack(0);
            }
        }

        static size_t PopCount(uint64_t x) noexcept
        {
#if defined(__GNUC__) || defined(__clang__)
            return static_cast<size_t>(__builtin_popcountll(x));
#elif defined(_MSC_VER)
            return static_cast<size_t>(__popcnt64(x));
#else
            // Fallback: Brian Kernighan's algorithm
            size_t count = 0;
            while (x)
            {
                x &= (x - 1);
                ++count;
            }
            return count;
#endif
        }

        static size_t CountTrailingZeros(uint64_t x) noexcept
        {
            if (x == 0) return 64;
#if defined(__GNUC__) || defined(__clang__)
            return static_cast<size_t>(__builtin_ctzll(x));
#elif defined(_MSC_VER)
            unsigned long index;
            _BitScanForward64(&index, x);
            return static_cast<size_t>(index);
#else
            size_t count = 0;
            while ((x & 1) == 0)
            {
                x >>= 1;
                ++count;
            }
            return count;
#endif
        }

        static size_t CountLeadingZeros(uint64_t x) noexcept
        {
            if (x == 0) return 64;
#if defined(__GNUC__) || defined(__clang__)
            return static_cast<size_t>(__builtin_clzll(x));
#elif defined(_MSC_VER)
            unsigned long index;
            _BitScanReverse64(&index, x);
            return static_cast<size_t>(63 - index);
#else
            size_t count = 0;
            uint64_t mask = uint64_t{1} << 63;
            while ((x & mask) == 0)
            {
                mask >>= 1;
                ++count;
            }
            return count;
#endif
        }

        Allocator* allocator_;
        wax::Vector<uint64_t, Allocator> blocks_;
    };

    /**
     * Create intersection of two masks
     */
    template<comb::Allocator Allocator>
    [[nodiscard]] ComponentMask<Allocator> operator&(ComponentMask<Allocator> lhs, const ComponentMask<Allocator>& rhs)
    {
        lhs &= rhs;
        return lhs;
    }

    /**
     * Create union of two masks
     */
    template<comb::Allocator Allocator>
    [[nodiscard]] ComponentMask<Allocator> operator|(ComponentMask<Allocator> lhs, const ComponentMask<Allocator>& rhs)
    {
        lhs |= rhs;
        return lhs;
    }

    /**
     * Create symmetric difference of two masks
     */
    template<comb::Allocator Allocator>
    [[nodiscard]] ComponentMask<Allocator> operator^(ComponentMask<Allocator> lhs, const ComponentMask<Allocator>& rhs)
    {
        lhs ^= rhs;
        return lhs;
    }
}
