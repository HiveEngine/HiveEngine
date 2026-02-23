#pragma once

#include <algorithm>
#include <cstddef>
#include <new>

namespace hive
{

template<class>
struct MaxSizeof;

template<class... Ts>
struct MaxSizeof<std::tuple<Ts...>>
{
    static constexpr auto value = (std::max)({sizeof(Ts)...});
};

template<class T>
constexpr auto kMaxSizeof = MaxSizeof<T>::value;

template<class>
struct MaxAlignof;

template<class... Ts>
struct MaxAlignof<std::tuple<Ts...>>
{
    static constexpr auto value = (std::max)({alignof(Ts)...});
};

template<class T>
constexpr auto kMaxAlignof = MaxAlignof<T>::value;

template<typename R, typename... Args>
class Functor
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
        explicit FreeFunction(R (*fn)(Args...)) noexcept : fn_{fn} {}

        R Invoke(Args&&... args) override {
            return fn_(static_cast<Args&&>(args)...);
        }

        FreeFunction* Clone(void* dst) const override {
            return ::new(dst) FreeFunction{*this};
        }

    private:
        R (*fn_)(Args...);
    };

    template<class T>
    class MemberFunction : public Callable
    {
    public:
        MemberFunction(T* obj, R (T::*fn)(Args...)) noexcept : obj_{obj}, fn_{fn} {}

        R Invoke(Args&&... args) override {
            return (obj_->*fn_)(static_cast<Args&&>(args)...);
        }

        MemberFunction* Clone(void* dst) const override {
            return ::new(dst) MemberFunction{*this};
        }

    private:
        T* obj_;
        R (T::*fn_)(Args...);
    };

    template<class T>
    class ConstMemberFunction : public Callable
    {
    public:
        ConstMemberFunction(T* obj, R (T::*fn)(Args...) const) noexcept : obj_{obj}, fn_{fn} {}

        R Invoke(Args&&... args) override {
            return (obj_->*fn_)(static_cast<Args&&>(args)...);
        }

        ConstMemberFunction* Clone(void* dst) const override {
            return ::new(dst) ConstMemberFunction{*this};
        }

    private:
        T* obj_;
        R (T::*fn_)(Args...) const;
    };

    using Types = std::tuple<
        FreeFunction,
        MemberFunction<Functor<R, Args...>>,
        ConstMemberFunction<Functor<R, Args...>>
    >;

    alignas(kMaxAlignof<Types>) char buffer_[kMaxSizeof<Types>];
    Callable* callable_{};

public:
    [[nodiscard]] bool IsEmpty() const noexcept { return !callable_; }

    Functor() = default;

    Functor(R (*fn)(Args...))
        : callable_{::new(buffer_) FreeFunction{fn}}
    {}

    template<class T>
    Functor(T* obj, R (T::*fn)(Args...))
        : callable_{::new(buffer_) MemberFunction<T>{obj, fn}}
    {}

    template<class T>
    Functor(T* obj, R (T::*fn)(Args...) const)
        : callable_{::new(buffer_) ConstMemberFunction<T>{obj, fn}}
    {}

    Functor(const Functor& other)
        : callable_{other.IsEmpty() ? nullptr : other.callable_->Clone(buffer_)}
    {}

    Functor& operator=(const Functor& other) {
        if (this != &other)
        {
            if (callable_)
                callable_->~Callable();
            callable_ = other.IsEmpty() ? nullptr : other.callable_->Clone(buffer_);
        }
        return *this;
    }

    ~Functor() {
        if (callable_)
            callable_->~Callable();
    }

    R operator()(Args... args) {
        return callable_->Invoke(static_cast<Args&&>(args)...);
    }
};

} // namespace hive
