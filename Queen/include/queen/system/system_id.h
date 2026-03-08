#pragma once

#include <cstdint>

namespace queen
{
    /**
     * Type-safe identifier for systems
     *
     * SystemId uniquely identifies a registered system within a World.
     * Uses a 32-bit index internally for compact storage and fast lookup.
     *
     * Performance characteristics:
     * - Comparison: O(1) - single integer compare
     * - Hashing: O(1) - direct value use
     * - Storage: 4 bytes
     *
     * Example:
     * @code
     *   SystemId movement = world.system<Read<Position>>("Movement").each(...);
     *   world.run_system(movement);
     * @endcode
     */
    class SystemId
    {
    public:
        constexpr SystemId() noexcept
            : m_index{kInvalidIndex}
        {
        }

        constexpr explicit SystemId(uint32_t index) noexcept
            : m_index{index}
        {
        }

        [[nodiscard]] constexpr uint32_t Index() const noexcept
        {
            return m_index;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_index != kInvalidIndex;
        }

        [[nodiscard]] static constexpr SystemId Invalid() noexcept
        {
            return SystemId{};
        }

        constexpr bool operator==(const SystemId& other) const noexcept = default;

        constexpr bool operator<(const SystemId& other) const noexcept
        {
            return m_index < other.m_index;
        }

    private:
        static constexpr uint32_t kInvalidIndex = ~uint32_t{0};
        uint32_t m_index;
    };
} // namespace queen
