#pragma once

#include <queen/core/type_id.h>
#include <wax/containers/vector.h>
#include <comb/allocator_concepts.h>

namespace queen
{
    /**
     * World access level for a system
     */
    enum class WorldAccess : uint8_t
    {
        None,       // No direct world access
        Read,       // Read-only world access
        Write,      // Read-write world access
        Exclusive,  // Exclusive access (blocks all other systems)
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
    template<comb::Allocator Allocator>
    class AccessDescriptor
    {
    public:
        explicit AccessDescriptor(Allocator& allocator)
            : component_reads_{allocator}
            , component_writes_{allocator}
            , resource_reads_{allocator}
            , resource_writes_{allocator}
            , world_access_{WorldAccess::None}
        {
        }

        template<typename T>
        void AddComponentRead()
        {
            AddUnique(component_reads_, TypeIdOf<T>());
        }

        template<typename T>
        void AddComponentWrite()
        {
            AddUnique(component_writes_, TypeIdOf<T>());
        }

        template<typename T>
        void AddResourceRead()
        {
            AddUnique(resource_reads_, TypeIdOf<T>());
        }

        template<typename T>
        void AddResourceWrite()
        {
            AddUnique(resource_writes_, TypeIdOf<T>());
        }

        void AddComponentRead(TypeId type_id)
        {
            AddUnique(component_reads_, type_id);
        }

        void AddComponentWrite(TypeId type_id)
        {
            AddUnique(component_writes_, type_id);
        }

        void AddResourceRead(TypeId type_id)
        {
            AddUnique(resource_reads_, type_id);
        }

        void AddResourceWrite(TypeId type_id)
        {
            AddUnique(resource_writes_, type_id);
        }

        void SetWorldAccess(WorldAccess access) noexcept
        {
            world_access_ = access;
        }

        [[nodiscard]] WorldAccess GetWorldAccess() const noexcept
        {
            return world_access_;
        }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& ComponentReads() const noexcept
        {
            return component_reads_;
        }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& ComponentWrites() const noexcept
        {
            return component_writes_;
        }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& ResourceReads() const noexcept
        {
            return resource_reads_;
        }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& ResourceWrites() const noexcept
        {
            return resource_writes_;
        }

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
        [[nodiscard]] bool ConflictsWith(const AccessDescriptor& other) const noexcept
        {
            if (world_access_ == WorldAccess::Exclusive ||
                other.world_access_ == WorldAccess::Exclusive)
            {
                return true;
            }

            if (HasOverlap(component_writes_, other.component_reads_) ||
                HasOverlap(component_writes_, other.component_writes_) ||
                HasOverlap(component_reads_, other.component_writes_))
            {
                return true;
            }

            if (HasOverlap(resource_writes_, other.resource_reads_) ||
                HasOverlap(resource_writes_, other.resource_writes_) ||
                HasOverlap(resource_reads_, other.resource_writes_))
            {
                return true;
            }

            return false;
        }

        /**
         * Check if this descriptor is empty (no access)
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return component_reads_.IsEmpty() &&
                   component_writes_.IsEmpty() &&
                   resource_reads_.IsEmpty() &&
                   resource_writes_.IsEmpty() &&
                   world_access_ == WorldAccess::None;
        }

        /**
         * Check if this is a pure system (no ECS data access)
         */
        [[nodiscard]] bool IsPure() const noexcept
        {
            return IsEmpty();
        }

        /**
         * Check if this system requires exclusive access
         */
        [[nodiscard]] bool IsExclusive() const noexcept
        {
            return world_access_ == WorldAccess::Exclusive;
        }

    private:
        static void AddUnique(wax::Vector<TypeId, Allocator>& vec, TypeId type_id)
        {
            for (size_t i = 0; i < vec.Size(); ++i)
            {
                if (vec[i] == type_id)
                {
                    return;
                }
            }
            vec.PushBack(type_id);
        }

        static bool HasOverlap(const wax::Vector<TypeId, Allocator>& a,
                               const wax::Vector<TypeId, Allocator>& b) noexcept
        {
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

        wax::Vector<TypeId, Allocator> component_reads_;
        wax::Vector<TypeId, Allocator> component_writes_;
        wax::Vector<TypeId, Allocator> resource_reads_;
        wax::Vector<TypeId, Allocator> resource_writes_;
        WorldAccess world_access_;
    };
}
