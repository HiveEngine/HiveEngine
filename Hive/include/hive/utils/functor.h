#pragma once

#include <algorithm>
#include <cstddef>
#include <new>
#include <tuple>

namespace hive
{

    template <class> struct MaxSizeof;

    template <class... Ts> struct MaxSizeof<std::tuple<Ts...>>
    {
        static constexpr auto value = (std::max)({sizeof(Ts)...});
    };

    template <class T> inline constexpr auto kMaxSizeof = MaxSizeof<T>::value;

    template <class> struct MaxAlignof;

    template <class... Ts> struct MaxAlignof<std::tuple<Ts...>>
    {
        static constexpr auto value = (std::max)({alignof(Ts)...});
    };

    template <class T> inline constexpr auto kMaxAlignof = MaxAlignof<T>::value;

    template <typename R, typename... Args> class Functor
    {
        struct Callable
        {
            Callable() = default;
            Callable(const Callable&) = default;
            Callable& operator=(const Callable&) = default;
            virtual ~Callable() = default;

            virtual R Invoke(Args&&...) = 0;
            virtual Callable* Clone(void*) const = 0;
        };

        class FreeFunction : public Callable
        {
        public:
            explicit FreeFunction(R (*fn)(Args...)) noexcept
                : m_fn{fn}
            {
            }

            R Invoke(Args&&... args) override
            {
                return m_fn(static_cast<Args&&>(args)...);
            }

            FreeFunction* Clone(void* dst) const override
            {
                return ::new (dst) FreeFunction{*this};
            }

        private:
            R (*m_fn)(Args...);
        };

        template <class T> class MemberFunction : public Callable
        {
        public:
            MemberFunction(T* obj, R (T::*fn)(Args...)) noexcept
                : m_obj{obj}
                , m_fn{fn}
            {
            }

            R Invoke(Args&&... args) override
            {
                return (m_obj->*m_fn)(static_cast<Args&&>(args)...);
            }

            MemberFunction* Clone(void* dst) const override
            {
                return ::new (dst) MemberFunction{*this};
            }

        private:
            T* m_obj;
            R (T::*m_fn)(Args...);
        };

        template <class T> class ConstMemberFunction : public Callable
        {
        public:
            ConstMemberFunction(T* obj, R (T::*fn)(Args...) const) noexcept
                : m_obj{obj}
                , m_fn{fn}
            {
            }

            R Invoke(Args&&... args) override
            {
                return (m_obj->*m_fn)(static_cast<Args&&>(args)...);
            }

            ConstMemberFunction* Clone(void* dst) const override
            {
                return ::new (dst) ConstMemberFunction{*this};
            }

        private:
            T* m_obj;
            R (T::*m_fn)(Args...) const;
        };

        using Types =
            std::tuple<FreeFunction, MemberFunction<Functor<R, Args...>>, ConstMemberFunction<Functor<R, Args...>>>;

        alignas(kMaxAlignof<Types>) char m_buffer[kMaxSizeof<Types>];
        Callable* m_callable{};

    public:
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return !m_callable;
        }

        Functor() = default;

        Functor(R (*fn)(Args...))
            : m_callable{::new (m_buffer) FreeFunction{fn}}
        {
        }

        template <class T>
        Functor(T* obj, R (T::*fn)(Args...))
            : m_callable{::new (m_buffer) MemberFunction<T>{obj, fn}}
        {
        }

        template <class T>
        Functor(T* obj, R (T::*fn)(Args...) const)
            : m_callable{::new (m_buffer) ConstMemberFunction<T>{obj, fn}}
        {
        }

        Functor(const Functor& other)
            : m_callable{other.IsEmpty() ? nullptr : other.m_callable->Clone(m_buffer)}
        {
        }

        Functor& operator=(const Functor& other)
        {
            if (this != &other)
            {
                if (m_callable)
                    m_callable->~Callable();
                m_callable = other.IsEmpty() ? nullptr : other.m_callable->Clone(m_buffer);
            }
            return *this;
        }

        ~Functor()
        {
            if (m_callable)
                m_callable->~Callable();
        }

        R operator()(Args... args)
        {
            return m_callable->Invoke(static_cast<Args&&>(args)...);
        }
    };

} // namespace hive
