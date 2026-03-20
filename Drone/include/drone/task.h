#pragma once

#include <hive/core/assert.h>

#include <drone/coroutine_allocator.h>

#include <coroutine>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace drone
{
    // Lazy coroutine. Starts suspended, executes when co_awaited.
    // Uses symmetric transfer to avoid stack overflow on deep chains.
    // Frames allocated from drone::GetCoroutineAllocator() (slab pool).
    template <typename T = void> class Task;

    namespace detail
    {
        struct PromiseBase
        {
            std::coroutine_handle<> m_continuation{};

            static void* operator new(size_t size)
            {
                return GetCoroutineAllocator().Allocate(size, alignof(std::max_align_t));
            }

            static void operator delete(void* ptr, size_t)
            {
                GetCoroutineAllocator().Deallocate(ptr);
            }

            auto initial_suspend() noexcept
            {
                return std::suspend_always{};
            }

            auto final_suspend() noexcept
            {
                struct Awaiter
                {
                    bool await_ready() noexcept
                    {
                        return false;
                    }
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept
                    {
                        // Symmetric transfer: avoids stack buildup on deep chains
                        auto cont = m_promise->m_continuation;
                        return cont ? cont : std::noop_coroutine();
                    }
                    void await_resume() noexcept
                    {
                    }
                    PromiseBase* m_promise;
                };
                return Awaiter{this};
            }

            void unhandled_exception() noexcept
            {
                std::abort();
            }
        };
    } // namespace detail

    // --- Task<T> for non-void return types ---

    template <typename T> class Task
    {
    public:
        struct promise_type : detail::PromiseBase
        {
            T m_value;

            Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            void return_value(T value) noexcept
            {
                m_value = static_cast<T&&>(value);
            }
        };

        using Handle = std::coroutine_handle<promise_type>;

        Task() noexcept
            : m_handle{nullptr}
        {
        }

        explicit Task(Handle handle) noexcept
            : m_handle{handle}
        {
        }

        ~Task()
        {
            if (m_handle)
                m_handle.destroy();
        }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        Task(Task&& other) noexcept
            : m_handle{other.m_handle}
        {
            other.m_handle = nullptr;
        }

        Task& operator=(Task&& other) noexcept
        {
            if (this != &other)
            {
                if (m_handle)
                    m_handle.destroy();
                m_handle = other.m_handle;
                other.m_handle = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsReady() const noexcept
        {
            return m_handle && m_handle.done();
        }

        [[nodiscard]] T& Result() &
        {
            hive::Assert(m_handle && m_handle.done(), "Task not complete");
            return m_handle.promise().m_value;
        }

        [[nodiscard]] T&& Result() &&
        {
            hive::Assert(m_handle && m_handle.done(), "Task not complete");
            return static_cast<T&&>(m_handle.promise().m_value);
        }

        auto operator co_await() const noexcept
        {
            struct Awaiter
            {
                Handle m_handle;

                bool await_ready() noexcept
                {
                    return m_handle.done();
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept
                {
                    m_handle.promise().m_continuation = caller;
                    return m_handle;
                }

                T await_resume()
                {
                    return static_cast<T&&>(m_handle.promise().m_value);
                }
            };
            return Awaiter{m_handle};
        }

    private:
        Handle m_handle;
    };

    // --- Task<void> specialization ---

    template <> class Task<void>
    {
    public:
        struct promise_type : detail::PromiseBase
        {
            Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            void return_void() noexcept
            {
            }
        };

        using Handle = std::coroutine_handle<promise_type>;

        Task() noexcept
            : m_handle{nullptr}
        {
        }

        explicit Task(Handle handle) noexcept
            : m_handle{handle}
        {
        }

        ~Task()
        {
            if (m_handle)
                m_handle.destroy();
        }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        Task(Task&& other) noexcept
            : m_handle{other.m_handle}
        {
            other.m_handle = nullptr;
        }

        Task& operator=(Task&& other) noexcept
        {
            if (this != &other)
            {
                if (m_handle)
                    m_handle.destroy();
                m_handle = other.m_handle;
                other.m_handle = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool IsReady() const noexcept
        {
            return m_handle && m_handle.done();
        }

        auto operator co_await() const noexcept
        {
            struct Awaiter
            {
                Handle m_handle;

                bool await_ready() noexcept
                {
                    return m_handle.done();
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept
                {
                    m_handle.promise().m_continuation = caller;
                    return m_handle;
                }

                void await_resume() noexcept
                {
                }
            };
            return Awaiter{m_handle};
        }

    private:
        Handle m_handle;
    };

    // Bridge: blocks caller until the task completes. Use at coroutine/non-coroutine boundary.
    template <typename T> T SyncWait(Task<T>& task)
    {
        hive::Assert(!task.IsReady(), "SyncWait on already-complete task");
        auto awaiter = task.operator co_await();
        awaiter.m_handle.resume();
        hive::Assert(awaiter.m_handle.done(), "SyncWait: task suspended (needs async scheduler)");
        return static_cast<T&&>(awaiter.m_handle.promise().m_value);
    }

    inline void SyncWait(Task<void>& task)
    {
        hive::Assert(!task.IsReady(), "SyncWait on already-complete task");
        auto awaiter = task.operator co_await();
        awaiter.m_handle.resume();
        hive::Assert(awaiter.m_handle.done(), "SyncWait: task suspended (needs async scheduler)");
    }

} // namespace drone
