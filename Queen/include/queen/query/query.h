#pragma once

#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <queen/core/tick.h>
#include <queen/query/query_term.h>
#include <queen/query/query_descriptor.h>
#include <queen/query/change_filter.h>
#include <queen/storage/archetype.h>
#include <queen/storage/component_index.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>

namespace queen
{
    namespace detail
    {
        template<typename... Terms>
        struct FilterDataTerms;

        template<>
        struct FilterDataTerms<>
        {
            using type = std::tuple<>;
        };

        template<typename First, typename... Rest>
        struct FilterDataTerms<First, Rest...>
        {
            using rest_type = typename FilterDataTerms<Rest...>::type;

            // Include term if it has data access AND is not a change filter
            // Change filters provide filtering, not data access
            using type = std::conditional_t<
                First::access != TermAccess::None && !IsChangeFilterV<First>,
                decltype(std::tuple_cat(std::declval<std::tuple<First>>(), std::declval<rest_type>())),
                rest_type
            >;
        };

        template<typename Tuple>
        struct TupleSize;

        template<typename... Ts>
        struct TupleSize<std::tuple<Ts...>>
        {
            static constexpr size_t value = sizeof...(Ts);
        };

        // Extract change filter terms from query terms
        template<typename... Terms>
        struct FilterChangeTerms;

        template<>
        struct FilterChangeTerms<>
        {
            using type = std::tuple<>;
        };

        template<typename First, typename... Rest>
        struct FilterChangeTerms<First, Rest...>
        {
            using rest_type = typename FilterChangeTerms<Rest...>::type;

            using type = std::conditional_t<
                IsChangeFilterV<First>,
                decltype(std::tuple_cat(std::declval<std::tuple<First>>(), std::declval<rest_type>())),
                rest_type
            >;
        };

        // Check if any term is a change filter
        template<typename... Terms>
        struct HasChangeFilter;

        template<>
        struct HasChangeFilter<>
        {
            static constexpr bool value = false;
        };

        template<typename First, typename... Rest>
        struct HasChangeFilter<First, Rest...>
        {
            static constexpr bool value = IsChangeFilterV<First> || HasChangeFilter<Rest...>::value;
        };

        template<typename... Terms>
        constexpr bool HasChangeFilterV = HasChangeFilter<Terms...>::value;

        // Get ticks pointer for a change filter term
        template<typename Allocator, typename Archetype, typename ChangeFilterTerm>
        const ComponentTicks* GetTicksPtr(Archetype* arch)
        {
            using ComponentT = typename ChangeFilterTerm::ComponentType;
            auto* column = arch->template GetColumn<ComponentT>();
            if (column == nullptr) return nullptr;
            return column->TicksData();
        }

        // Forward declaration
        template<size_t I, typename TicksTuple, typename... ChangeFilterTerms>
        bool CheckAllFiltersImpl(const TicksTuple& ticks_ptrs, size_t row, Tick last_run,
                                 std::tuple<ChangeFilterTerms...>);

        template<size_t I, typename TicksTuple, typename... ChangeFilterTerms>
        bool CheckAllFiltersImpl(const TicksTuple& ticks_ptrs, size_t row, Tick last_run,
                                 std::tuple<ChangeFilterTerms...>)
        {
            if constexpr (I >= sizeof...(ChangeFilterTerms))
            {
                return true;
            }
            else
            {
                using FilterTerm = std::tuple_element_t<I, std::tuple<ChangeFilterTerms...>>;
                const ComponentTicks* ticks = std::get<I>(ticks_ptrs);

                if (ticks == nullptr) return false;

                auto filter = FilterTerm::ToChangeFilter();
                if (!filter.Matches(ticks[row], last_run))
                {
                    return false;
                }

                return CheckAllFiltersImpl<I + 1>(ticks_ptrs, row, last_run, std::tuple<ChangeFilterTerms...>{});
            }
        }

        // Check if a row passes all change filters
        template<typename Allocator, typename Archetype, typename... ChangeFilterTerms>
        bool PassesChangeFilters(
            Archetype* arch,
            size_t row,
            Tick last_run,
            std::tuple<ChangeFilterTerms...>)
        {
            if constexpr (sizeof...(ChangeFilterTerms) == 0)
            {
                return true;
            }
            else
            {
                // Get ticks for each change filter component
                auto ticks_ptrs = std::make_tuple(
                    GetTicksPtr<Allocator, Archetype, ChangeFilterTerms>(arch)...
                );

                // Check all filters
                return CheckAllFiltersImpl<0>(ticks_ptrs, row, last_run, std::tuple<ChangeFilterTerms...>{});
            }
        }

        template<typename Allocator, typename Archetype, typename Term>
        auto GetColumnPtr(Archetype* arch)
        {
            using ComponentT = typename Term::ComponentType;

            if constexpr (Term::op == TermOperator::Optional)
            {
                auto* column = arch->template GetColumn<ComponentT>();
                if (column == nullptr)
                {
                    return static_cast<ComponentT*>(nullptr);
                }
                return column->template Data<ComponentT>();
            }
            else
            {
                auto* column = arch->template GetColumn<ComponentT>();
                return column->template Data<ComponentT>();
            }
        }

        template<typename Term, typename ColumnPtr>
        decltype(auto) GetComponentRef(ColumnPtr ptr, size_t row)
        {
            using ComponentT = typename Term::ComponentType;

            if constexpr (Term::op == TermOperator::Optional)
            {
                if (ptr == nullptr)
                {
                    return static_cast<ComponentT*>(nullptr);
                }
                return &ptr[row];
            }
            else if constexpr (Term::access == TermAccess::Read)
            {
                return static_cast<const ComponentT&>(ptr[row]);
            }
            else
            {
                return static_cast<ComponentT&>(ptr[row]);
            }
        }
    }

    /**
     * Query result for iterating over matching entities
     *
     * Provides iteration over all entities that match a query's terms.
     * Components are accessed through the callback in Each().
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────┐
     * │ archetypes_: Vector<Archetype*> (matching archetypes)        │
     * │ descriptor_: QueryDescriptor (term definitions)              │
     * └──────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Each: O(n) where n = total matching entities across archetypes
     * - Iteration is cache-friendly within each archetype
     *
     * Limitations:
     * - Not thread-safe
     * - Cannot modify component set while iterating
     *
     * Example:
     * @code
     *   Query<Read<Position>, Write<Velocity>, Without<Dead>> query{alloc, index};
     *
     *   query.Each([](const Position& pos, Velocity& vel) {
     *       vel.dx += pos.x * 0.1f;
     *   });
     * @endcode
     */
    template<comb::Allocator Allocator, typename... Terms>
    class Query
    {
        using DataTerms = typename detail::FilterDataTerms<Terms...>::type;
        using ChangeFilterTerms = typename detail::FilterChangeTerms<Terms...>::type;
        static constexpr size_t DataTermCount = detail::TupleSize<DataTerms>::value;
        static constexpr size_t ChangeFilterCount = detail::TupleSize<ChangeFilterTerms>::value;
        static constexpr bool HasChangeFilters = detail::HasChangeFilterV<Terms...>;

    public:
        Query(Allocator& allocator, const ComponentIndex<Allocator>& index)
            : allocator_{&allocator}
            , archetypes_{allocator}
            , descriptor_{allocator}
            , last_run_tick_{0}
        {
            (descriptor_.template AddTerm<Terms>(), ...);
            descriptor_.Finalize();

            archetypes_ = descriptor_.FindMatchingArchetypes(index);
        }

        ~Query() = default;

        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;
        Query(Query&&) = default;
        Query& operator=(Query&&) = default;

        /**
         * Set the last_run_tick for change detection filtering
         *
         * When set, queries with Added<T>/Changed<T> filters will only
         * iterate entities where the component ticks match the filter.
         */
        void SetLastRunTick(Tick tick) noexcept
        {
            last_run_tick_ = tick;
        }

        [[nodiscard]] Tick LastRunTick() const noexcept
        {
            return last_run_tick_;
        }

        template<typename Func>
        void Each(Func&& func)
        {
            for (size_t a = 0; a < archetypes_.Size(); ++a)
            {
                Archetype<Allocator>* arch = archetypes_[a];
                EachInArchetype(arch, std::forward<Func>(func), DataTerms{}, ChangeFilterTerms{});
            }
        }

        template<typename Func>
        void EachWithEntity(Func&& func)
        {
            for (size_t a = 0; a < archetypes_.Size(); ++a)
            {
                Archetype<Allocator>* arch = archetypes_[a];
                EachInArchetypeWithEntity(arch, std::forward<Func>(func), DataTerms{}, ChangeFilterTerms{});
            }
        }

        [[nodiscard]] size_t ArchetypeCount() const noexcept
        {
            return archetypes_.Size();
        }

        [[nodiscard]] size_t EntityCount() const noexcept
        {
            size_t count = 0;
            for (size_t i = 0; i < archetypes_.Size(); ++i)
            {
                count += archetypes_[i]->EntityCount();
            }
            return count;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return EntityCount() == 0;
        }

    private:
        template<typename Func, typename... DataTermTypes, typename... ChangeFilterTypes>
        void EachInArchetype(Archetype<Allocator>* arch, Func&& func,
                            std::tuple<DataTermTypes...>, std::tuple<ChangeFilterTypes...>)
        {
            size_t count = arch->EntityCount();
            if (count == 0) return;

            auto columns = std::make_tuple(
                detail::GetColumnPtr<Allocator, Archetype<Allocator>, DataTermTypes>(arch)...
            );

            if constexpr (sizeof...(ChangeFilterTypes) == 0)
            {
                // No change filters - iterate all
                for (size_t row = 0; row < count; ++row)
                {
                    InvokeCallback(func, columns, row, std::make_index_sequence<sizeof...(DataTermTypes)>{},
                                   std::tuple<DataTermTypes...>{});
                }
            }
            else
            {
                // Has change filters - check each row
                for (size_t row = 0; row < count; ++row)
                {
                    if (detail::PassesChangeFilters<Allocator, Archetype<Allocator>>(
                            arch, row, last_run_tick_, std::tuple<ChangeFilterTypes...>{}))
                    {
                        InvokeCallback(func, columns, row, std::make_index_sequence<sizeof...(DataTermTypes)>{},
                                       std::tuple<DataTermTypes...>{});
                    }
                }
            }
        }

        template<typename Func, typename... DataTermTypes, typename... ChangeFilterTypes>
        void EachInArchetypeWithEntity(Archetype<Allocator>* arch, Func&& func,
                                       std::tuple<DataTermTypes...>, std::tuple<ChangeFilterTypes...>)
        {
            size_t count = arch->EntityCount();
            if (count == 0) return;

            const Entity* entities = arch->GetEntities();
            auto columns = std::make_tuple(
                detail::GetColumnPtr<Allocator, Archetype<Allocator>, DataTermTypes>(arch)...
            );

            if constexpr (sizeof...(ChangeFilterTypes) == 0)
            {
                // No change filters - iterate all
                for (size_t row = 0; row < count; ++row)
                {
                    InvokeCallbackWithEntity(func, entities[row], columns, row,
                                             std::make_index_sequence<sizeof...(DataTermTypes)>{},
                                             std::tuple<DataTermTypes...>{});
                }
            }
            else
            {
                // Has change filters - check each row
                for (size_t row = 0; row < count; ++row)
                {
                    if (detail::PassesChangeFilters<Allocator, Archetype<Allocator>>(
                            arch, row, last_run_tick_, std::tuple<ChangeFilterTypes...>{}))
                    {
                        InvokeCallbackWithEntity(func, entities[row], columns, row,
                                                 std::make_index_sequence<sizeof...(DataTermTypes)>{},
                                                 std::tuple<DataTermTypes...>{});
                    }
                }
            }
        }

        template<typename Func, typename Tuple, size_t... Is, typename... DataTermTypes>
        void InvokeCallback(Func& func, Tuple& columns, size_t row, std::index_sequence<Is...>,
                           std::tuple<DataTermTypes...>)
        {
            func(detail::GetComponentRef<DataTermTypes>(std::get<Is>(columns), row)...);
        }

        template<typename Func, typename Tuple, size_t... Is, typename... DataTermTypes>
        void InvokeCallbackWithEntity(Func& func, Entity entity, Tuple& columns, size_t row,
                                      std::index_sequence<Is...>, std::tuple<DataTermTypes...>)
        {
            func(entity, detail::GetComponentRef<DataTermTypes>(std::get<Is>(columns), row)...);
        }

        Allocator* allocator_;
        wax::Vector<Archetype<Allocator>*, Allocator> archetypes_;
        QueryDescriptor<Allocator> descriptor_;
        Tick last_run_tick_;
    };
}
