#pragma once

#include <queen/core/type_id.h>
#include <queen/query/query_term.h>
#include <queen/storage/archetype.h>
#include <queen/storage/component_index.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>

namespace queen
{
    /**
     * Query descriptor - runtime representation of a query
     *
     * Holds the terms that define which archetypes match a query.
     * Extracts required, excluded, and optional component TypeIds from
     * a list of terms for efficient archetype matching.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────┐
     * │ terms_: Vector<Term>         (all query terms)               │
     * │ required_: Vector<TypeId>    (With terms, must have)         │
     * │ excluded_: Vector<TypeId>    (Without terms, must not have)  │
     * │ optional_: Vector<TypeId>    (Maybe terms, may have)         │
     * │ data_access_: Vector<Term>   (terms with Read/Write access)  │
     * └──────────────────────────────────────────────────────────────┘
     *
     * Matching logic:
     * - Archetype must have ALL required components
     * - Archetype must have NONE of the excluded components
     * - Optional components are fetched if present (nullptr otherwise)
     *
     * Performance characteristics:
     * - Construction: O(n) where n = number of terms
     * - MatchesArchetype: O(r + e) where r=required, e=excluded
     *
     * Limitations:
     * - Not thread-safe
     * - Immutable after construction
     *
     * Example:
     * @code
     *   QueryDescriptor desc{alloc};
     *   desc.AddTerm(Read<Position>::ToTerm());
     *   desc.AddTerm(Write<Velocity>::ToTerm());
     *   desc.AddTerm(Without<Dead>::ToTerm());
     *   desc.Finalize();
     *
     *   if (desc.MatchesArchetype(*archetype)) {
     *       // archetype has Position, Velocity, and NOT Dead
     *   }
     * @endcode
     */
    template<comb::Allocator Allocator>
    class QueryDescriptor
    {
    public:
        explicit QueryDescriptor(Allocator& allocator)
            : allocator_{&allocator}
            , terms_{allocator}
            , required_{allocator}
            , excluded_{allocator}
            , optional_{allocator}
            , data_access_{allocator}
        {
        }

        ~QueryDescriptor() = default;

        QueryDescriptor(const QueryDescriptor&) = delete;
        QueryDescriptor& operator=(const QueryDescriptor&) = delete;
        QueryDescriptor(QueryDescriptor&&) = default;
        QueryDescriptor& operator=(QueryDescriptor&&) = default;

        void AddTerm(const Term& term)
        {
            terms_.PushBack(term);
        }

        template<typename TermWrapper>
        void AddTerm()
        {
            AddTerm(TermWrapper::ToTerm());
        }

        void Finalize()
        {
            required_.Clear();
            excluded_.Clear();
            optional_.Clear();
            data_access_.Clear();

            for (size_t i = 0; i < terms_.Size(); ++i)
            {
                const Term& term = terms_[i];

                switch (term.op)
                {
                    case TermOperator::With:
                        required_.PushBack(term.type_id);
                        break;
                    case TermOperator::Without:
                        excluded_.PushBack(term.type_id);
                        break;
                    case TermOperator::Optional:
                        optional_.PushBack(term.type_id);
                        break;
                }

                if (term.HasDataAccess())
                {
                    data_access_.PushBack(term);
                }
            }
        }

        template<typename... TermWrappers>
        static QueryDescriptor FromTerms(Allocator& allocator)
        {
            QueryDescriptor desc{allocator};
            (desc.template AddTerm<TermWrappers>(), ...);
            desc.Finalize();
            return desc;
        }

        [[nodiscard]] bool MatchesArchetype(const Archetype<Allocator>& archetype) const noexcept
        {
            for (size_t i = 0; i < required_.Size(); ++i)
            {
                if (!archetype.HasComponent(required_[i]))
                {
                    return false;
                }
            }

            for (size_t i = 0; i < excluded_.Size(); ++i)
            {
                if (archetype.HasComponent(excluded_[i]))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] wax::Vector<Archetype<Allocator>*, Allocator> FindMatchingArchetypes(
            const ComponentIndex<Allocator>& index) const
        {
            wax::Vector<Archetype<Allocator>*, Allocator> result{*allocator_};

            if (required_.IsEmpty())
            {
                return result;
            }

            size_t smallest_idx = 0;
            size_t smallest_size = SIZE_MAX;

            for (size_t i = 0; i < required_.Size(); ++i)
            {
                const auto* list = index.GetArchetypesWith(required_[i]);
                if (list == nullptr)
                {
                    return result;
                }
                if (list->Size() < smallest_size)
                {
                    smallest_size = list->Size();
                    smallest_idx = i;
                }
            }

            const auto* candidates = index.GetArchetypesWith(required_[smallest_idx]);
            if (candidates == nullptr)
            {
                return result;
            }

            for (size_t i = 0; i < candidates->Size(); ++i)
            {
                Archetype<Allocator>* arch = (*candidates)[i];
                if (MatchesArchetype(*arch))
                {
                    result.PushBack(arch);
                }
            }

            return result;
        }

        [[nodiscard]] const wax::Vector<Term, Allocator>& GetTerms() const noexcept
        {
            return terms_;
        }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& GetRequired() const noexcept
        {
            return required_;
        }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& GetExcluded() const noexcept
        {
            return excluded_;
        }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& GetOptional() const noexcept
        {
            return optional_;
        }

        [[nodiscard]] const wax::Vector<Term, Allocator>& GetDataAccessTerms() const noexcept
        {
            return data_access_;
        }

        [[nodiscard]] size_t TermCount() const noexcept { return terms_.Size(); }
        [[nodiscard]] size_t RequiredCount() const noexcept { return required_.Size(); }
        [[nodiscard]] size_t ExcludedCount() const noexcept { return excluded_.Size(); }
        [[nodiscard]] size_t OptionalCount() const noexcept { return optional_.Size(); }
        [[nodiscard]] size_t DataAccessCount() const noexcept { return data_access_.Size(); }

        [[nodiscard]] bool IsEmpty() const noexcept { return terms_.IsEmpty(); }
        [[nodiscard]] bool HasRequired() const noexcept { return !required_.IsEmpty(); }
        [[nodiscard]] bool HasExcluded() const noexcept { return !excluded_.IsEmpty(); }
        [[nodiscard]] bool HasOptional() const noexcept { return !optional_.IsEmpty(); }

    private:
        Allocator* allocator_;
        wax::Vector<Term, Allocator> terms_;
        wax::Vector<TypeId, Allocator> required_;
        wax::Vector<TypeId, Allocator> excluded_;
        wax::Vector<TypeId, Allocator> optional_;
        wax::Vector<Term, Allocator> data_access_;
    };
}
