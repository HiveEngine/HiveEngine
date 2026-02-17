#pragma once

#include <queen/system/system_id.h>
#include <queen/system/access_descriptor.h>
#include <queen/query/query_descriptor.h>
#include <queen/core/tick.h>
#include <comb/allocator_concepts.h>
#include <cstring>

namespace queen
{
    class World;

    /**
     * Execution mode for a system
     */
    enum class SystemExecutor : uint8_t
    {
        Sequential, // Runs on main thread only
        Parallel,   // Can run with non-conflicting systems
        Exclusive,  // Requires exclusive world access
    };

    /**
     * Type-erased system executor function
     *
     * The executor is a type-erased callable that executes the system logic.
     * It receives a World reference and performs the system's work.
     */
    using SystemExecutorFn = void (*)(World& world, void* user_data);

    /**
     * Describes a registered system
     *
     * SystemDescriptor contains all metadata needed to schedule and execute
     * a system. This includes the system's name, access pattern, query,
     * and the type-erased executor function.
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ id_: SystemId                                                   │
     * │ name_: FixedString<64>                                          │
     * │ access_: AccessDescriptor                                       │
     * │ query_: QueryDescriptor (optional)                              │
     * │ executor_fn_: function pointer                                  │
     * │ user_data_: void* (for lambda captures)                         │
     * │ executor_mode_: SystemExecutor                                  │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Execution: O(1) function call + query iteration
     * - Name lookup: O(n) linear search (use SystemId for fast access)
     *
     * Limitations:
     * - System name limited to 64 characters
     * - Query must be set before execution for query-based systems
     *
     * Example:
     * @code
     *   // Created automatically via World::system<>()
     *   SystemId id = world.system<Read<Position>>("Movement")
     *       .each([](const Position& pos) { ... });
     * @endcode
     */
    template<comb::Allocator Allocator>
    class SystemDescriptor
    {
    public:
        static constexpr size_t kMaxNameLength = 63;

        SystemDescriptor(Allocator& allocator, SystemId id, const char* name)
            : id_{id}
            , allocator_{&allocator}
            , access_{allocator}
            , query_{allocator}
            , executor_fn_{nullptr}
            , user_data_{nullptr}
            , destructor_fn_{nullptr}
            , executor_mode_{SystemExecutor::Parallel}
            , enabled_{true}
        {
            if (name != nullptr)
            {
                size_t len = std::strlen(name);
                if (len > kMaxNameLength) len = kMaxNameLength;
                std::memcpy(name_, name, len);
                name_[len] = '\0';
            }
            else
            {
                name_[0] = '\0';
            }
        }

        ~SystemDescriptor()
        {
            if (user_data_ != nullptr)
            {
                if (destructor_fn_ != nullptr)
                {
                    destructor_fn_(user_data_);
                }
                allocator_->Deallocate(user_data_);
            }
        }

        SystemDescriptor(const SystemDescriptor&) = delete;
        SystemDescriptor& operator=(const SystemDescriptor&) = delete;
        SystemDescriptor(SystemDescriptor&& other) noexcept
            : id_{other.id_}
            , allocator_{other.allocator_}
            , access_{std::move(other.access_)}
            , query_{std::move(other.query_)}
            , executor_fn_{other.executor_fn_}
            , user_data_{other.user_data_}
            , destructor_fn_{other.destructor_fn_}
            , executor_mode_{other.executor_mode_}
            , enabled_{other.enabled_}
            , last_run_tick_{other.last_run_tick_}
        {
            std::memcpy(name_, other.name_, sizeof(name_));
            other.user_data_ = nullptr;
            other.destructor_fn_ = nullptr;
        }

        SystemDescriptor& operator=(SystemDescriptor&& other) noexcept
        {
            if (this != &other)
            {
                if (user_data_ != nullptr)
                {
                    if (destructor_fn_ != nullptr)
                    {
                        destructor_fn_(user_data_);
                    }
                    allocator_->Deallocate(user_data_);
                }

                id_ = other.id_;
                allocator_ = other.allocator_;
                std::memcpy(name_, other.name_, sizeof(name_));
                access_ = std::move(other.access_);
                query_ = std::move(other.query_);
                executor_fn_ = other.executor_fn_;
                user_data_ = other.user_data_;
                destructor_fn_ = other.destructor_fn_;
                executor_mode_ = other.executor_mode_;
                enabled_ = other.enabled_;
                last_run_tick_ = other.last_run_tick_;

                other.user_data_ = nullptr;
                other.destructor_fn_ = nullptr;
            }
            return *this;
        }

        [[nodiscard]] SystemId Id() const noexcept { return id_; }
        [[nodiscard]] const char* Name() const noexcept { return name_; }
        [[nodiscard]] const AccessDescriptor<Allocator>& Access() const noexcept { return access_; }
        [[nodiscard]] AccessDescriptor<Allocator>& Access() noexcept { return access_; }
        [[nodiscard]] const QueryDescriptor<Allocator>& Query() const noexcept { return query_; }
        [[nodiscard]] QueryDescriptor<Allocator>& Query() noexcept { return query_; }
        [[nodiscard]] SystemExecutor ExecutorMode() const noexcept { return executor_mode_; }
        [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }
        [[nodiscard]] Tick LastRunTick() const noexcept { return last_run_tick_; }

        void SetExecutorMode(SystemExecutor mode) noexcept
        {
            executor_mode_ = mode;
            if (mode == SystemExecutor::Exclusive)
            {
                access_.SetWorldAccess(WorldAccess::Exclusive);
            }
        }

        void SetEnabled(bool enabled) noexcept
        {
            enabled_ = enabled;
        }

        void SetExecutor(SystemExecutorFn fn, void* user_data, void (*destructor)(void*))
        {
            if (user_data_ != nullptr)
            {
                if (destructor_fn_ != nullptr)
                {
                    destructor_fn_(user_data_);
                }
                allocator_->Deallocate(user_data_);
            }
            executor_fn_ = fn;
            user_data_ = user_data;
            destructor_fn_ = destructor;
        }

        /**
         * Execute the system and update last_run_tick
         *
         * @param world The world to execute on
         * @param current_tick The current world tick (for change detection)
         */
        void Execute(World& world, Tick current_tick)
        {
            if (executor_fn_ != nullptr && enabled_)
            {
                executor_fn_(world, user_data_);
                last_run_tick_ = current_tick;
            }
        }


        [[nodiscard]] bool HasExecutor() const noexcept
        {
            return executor_fn_ != nullptr;
        }

    private:
        SystemId id_;
        Allocator* allocator_;
        char name_[kMaxNameLength + 1];
        AccessDescriptor<Allocator> access_;
        QueryDescriptor<Allocator> query_;
        SystemExecutorFn executor_fn_;
        void* user_data_;
        void (*destructor_fn_)(void*);
        SystemExecutor executor_mode_;
        bool enabled_;
        Tick last_run_tick_{0}; // Tick when this system last ran (0 = never)
    };
}
