#pragma once

#include <queen/core/type_id.h>

namespace queen
{
    /**
     * Immutable resource reference for system parameters
     *
     * Res<T> provides read-only access to a global resource. It is used
     * as a system parameter to declare resource dependencies.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────────┐
     * │ ptr_: const T* (pointer to resource)                             │
     * └──────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Access: O(1) - direct pointer dereference
     * - Construction: O(1) - single pointer copy
     * - Thread-safe: Read access only (safe for parallel systems)
     *
     * Use cases:
     * - Reading configuration resources
     * - Accessing shared data (e.g., Time, Input state)
     * - Declaring read dependencies for scheduling
     *
     * Limitations:
     * - Resource must exist when system runs (asserts if missing)
     * - Cannot modify the resource
     *
     * Example:
     * @code
     *   struct Time { float delta; float elapsed; };
     *
     *   world.InsertResource(Time{0.016f, 0.0f});
     *
     *   world.System<Read<Position>>("UpdateTimer")
     *       .RunWithRes([](Res<Time> time) {
     *           // time->delta, time->elapsed are read-only
     *       });
     * @endcode
     */
    template <typename T> class Res
    {
    public:
        using ValueType = T;
        static constexpr TypeId typeId = TypeIdOf<T>();
        static constexpr bool isMutable = false;

        explicit constexpr Res(const T* ptr) noexcept
            : m_ptr{ptr} {}

        [[nodiscard]] constexpr const T& operator*() const noexcept { return *m_ptr; }

        [[nodiscard]] constexpr const T* operator->() const noexcept { return m_ptr; }

        [[nodiscard]] constexpr const T* Get() const noexcept { return m_ptr; }

        [[nodiscard]] constexpr bool IsValid() const noexcept { return m_ptr != nullptr; }

        [[nodiscard]] explicit constexpr operator bool() const noexcept { return m_ptr != nullptr; }

    private:
        const T* m_ptr;
    };

    /**
     * Mutable resource reference for system parameters
     *
     * ResMut<T> provides read-write access to a global resource. It is used
     * as a system parameter to declare resource dependencies.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────────┐
     * │ ptr_: T* (pointer to resource)                                   │
     * └──────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Access: O(1) - direct pointer dereference
     * - Construction: O(1) - single pointer copy
     * - Thread-safe: No (write access creates conflicts)
     *
     * Use cases:
     * - Modifying shared state (e.g., updating Time)
     * - Accumulating data (e.g., statistics)
     * - Declaring write dependencies for scheduling
     *
     * Limitations:
     * - Resource must exist when system runs (asserts if missing)
     * - Creates scheduling conflicts with other ResMut<T> or Res<T>
     *
     * Example:
     * @code
     *   struct Time { float delta; float elapsed; };
     *
     *   world.InsertResource(Time{0.016f, 0.0f});
     *
     *   world.System("UpdateTimer")
     *       .RunWithResMut([](ResMut<Time> time) {
     *           time->elapsed += time->delta;
     *       });
     * @endcode
     */
    template <typename T> class ResMut
    {
    public:
        using ValueType = T;
        static constexpr TypeId typeId = TypeIdOf<T>();
        static constexpr bool isMutable = true;

        explicit constexpr ResMut(T* ptr) noexcept
            : m_ptr{ptr} {}

        [[nodiscard]] constexpr T& operator*() const noexcept { return *m_ptr; }

        [[nodiscard]] constexpr T* operator->() const noexcept { return m_ptr; }

        [[nodiscard]] constexpr T* Get() const noexcept { return m_ptr; }

        [[nodiscard]] constexpr bool IsValid() const noexcept { return m_ptr != nullptr; }

        [[nodiscard]] explicit constexpr operator bool() const noexcept { return m_ptr != nullptr; }

    private:
        T* m_ptr;
    };

    // Type traits for detecting Res/ResMut
    template <typename T> struct IsRes : std::false_type
    {
    };

    template <typename T> struct IsRes<Res<T>> : std::true_type
    {
    };

    template <typename T> inline constexpr bool isResV = IsRes<T>::value;

    template <typename T> struct IsResMut : std::false_type
    {
    };

    template <typename T> struct IsResMut<ResMut<T>> : std::true_type
    {
    };

    template <typename T> inline constexpr bool isResMutV = IsResMut<T>::value;

    template <typename T> inline constexpr bool isResourceParam = isResV<T> || isResMutV<T>;
} // namespace queen
