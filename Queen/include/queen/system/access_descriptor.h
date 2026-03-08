#pragma once

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/core/type_id.h>

namespace queen
{
    /**
     * World access level for a system
     */
    enum class WorldAccess : uint8_t
    {
        NONE,      // No direct world access
        READ,      // Read-only world access
        WRITE,     // Read-write world access
        EXCLUSIVE, // Exclusive access (blocks all other systems)
    };

    /**
     * Describes the data access pattern of a system
     *
     * AccessDescriptor captures which components and resources a system reads
     * and writes, enabling the scheduler to determine which systems can run
     * in parallel without data races.
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ component_reads_: Vector<TypeId>                                │
     * │ component_writes_: Vector<TypeId>                               │
     * │ resource_reads_: Vector<TypeId>                                 │
     * │ resource_writes_: Vector<TypeId>                                │
     * │ world_access_: WorldAccess                                      │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Conflict check: O(n*m) where n,m are access list sizes
     * - Adding access: O(n) to check duplicates
     * - Storage: ~96 bytes + vector contents
     *
     * Use cases:
     * - Automatic parallel scheduling
     * - Detecting data race hazards at runtime
     * - System dependency graph construction
     *
     * Example:
     * @code
     *   AccessDescriptor desc{alloc};
     *   desc.AddComponentRead<Position>();
     *   desc.AddComponentWrite<Velocity>();
     *   desc.AddResourceRead<Time>();
     *
     *   if (desc.ConflictsWith(other_desc)) {
     *       // Cannot run in parallel
     *   }
     * @endcode
     */
    template <comb::Allocator Allocator> class AccessDescriptor
    {
    public:
        explicit AccessDescriptor(Allocator& allocator)
            : m_componentReads{allocator}
            , m_componentWrites{allocator}
            , m_resourceReads{allocator}
            , m_resourceWrites{allocator}
            , m_worldAccess{WorldAccess::NONE} {}

        template <typename T> void AddComponentRead() { AddUnique(m_componentReads, TypeIdOf<T>()); }

        template <typename T> void AddComponentWrite() { AddUnique(m_componentWrites, TypeIdOf<T>()); }

        template <typename T> void AddResourceRead() { AddUnique(m_resourceReads, TypeIdOf<T>()); }

        template <typename T> void AddResourceWrite() { AddUnique(m_resourceWrites, TypeIdOf<T>()); }

        void AddComponentRead(TypeId typeId) { AddUnique(m_componentReads, typeId); }

        void AddComponentWrite(TypeId typeId) { AddUnique(m_componentWrites, typeId); }

        void AddResourceRead(TypeId typeId) { AddUnique(m_resourceReads, typeId); }

        void AddResourceWrite(TypeId typeId) { AddUnique(m_resourceWrites, typeId); }

        void SetWorldAccess(WorldAccess access) noexcept { m_worldAccess = access; }

        [[nodiscard]] WorldAccess GetWorldAccess() const noexcept { return m_worldAccess; }

        [[nodiscard]] const wax::Vector<TypeId>& ComponentReads() const noexcept { return m_componentReads; }

        [[nodiscard]] const wax::Vector<TypeId>& ComponentWrites() const noexcept { return m_componentWrites; }

        [[nodiscard]] const wax::Vector<TypeId>& ResourceReads() const noexcept { return m_resourceReads; }

        [[nodiscard]] const wax::Vector<TypeId>& ResourceWrites() const noexcept { return m_resourceWrites; }

        /**
         * Check if this descriptor conflicts with another
         *
         * Two systems conflict if:
         * - Either has exclusive world access
         * - One writes a component/resource that the other reads or writes
         *
         * @param other The other access descriptor
         * @return true if systems cannot run in parallel
         */
        [[nodiscard]] bool ConflictsWith(const AccessDescriptor& other) const noexcept {
            if (m_worldAccess == WorldAccess::EXCLUSIVE || other.m_worldAccess == WorldAccess::EXCLUSIVE)
            {
                return true;
            }

            if (HasOverlap(m_componentWrites, other.m_componentReads) ||
                HasOverlap(m_componentWrites, other.m_componentWrites) ||
                HasOverlap(m_componentReads, other.m_componentWrites))
            {
                return true;
            }

            if (HasOverlap(m_resourceWrites, other.m_resourceReads) ||
                HasOverlap(m_resourceWrites, other.m_resourceWrites) ||
                HasOverlap(m_resourceReads, other.m_resourceWrites))
            {
                return true;
            }

            return false;
        }

        /**
         * Check if this descriptor is empty (no access)
         */
        [[nodiscard]] bool IsEmpty() const noexcept {
            return m_componentReads.IsEmpty() && m_componentWrites.IsEmpty() && m_resourceReads.IsEmpty() &&
                   m_resourceWrites.IsEmpty() && m_worldAccess == WorldAccess::NONE;
        }

        /**
         * Check if this is a pure system (no ECS data access)
         */
        [[nodiscard]] bool IsPure() const noexcept { return IsEmpty(); }

        /**
         * Check if this system requires exclusive access
         */
        [[nodiscard]] bool IsExclusive() const noexcept { return m_worldAccess == WorldAccess::EXCLUSIVE; }

    private:
        static void AddUnique(wax::Vector<TypeId>& vec, TypeId typeId) {
            for (size_t i = 0; i < vec.Size(); ++i)
            {
                if (vec[i] == typeId)
                {
                    return;
                }
            }
            vec.PushBack(typeId);
        }

        static bool HasOverlap(const wax::Vector<TypeId>& a, const wax::Vector<TypeId>& b) noexcept {
            for (size_t i = 0; i < a.Size(); ++i)
            {
                for (size_t j = 0; j < b.Size(); ++j)
                {
                    if (a[i] == b[j])
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        wax::Vector<TypeId> m_componentReads;
        wax::Vector<TypeId> m_componentWrites;
        wax::Vector<TypeId> m_resourceReads;
        wax::Vector<TypeId> m_resourceWrites;
        WorldAccess m_worldAccess;
    };
} // namespace queen
