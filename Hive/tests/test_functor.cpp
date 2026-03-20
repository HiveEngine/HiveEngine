#include <hive/utils/functor.h>

#include <larvae/larvae.h>

namespace
{

    int Add(int a, int b) { return a + b; }

    void Increment(int& x) { ++x; }

    struct Widget
    {
        int m_value{0};

        void Set(int v) { m_value = v; }
        int Get() const { return m_value; }
        int Multiply(int factor) { return m_value *= factor; }
    };

    // Free functions

    auto t_free_fn = larvae::RegisterTest("HiveFunctor", "FreeFunctionCall", []() {
        hive::Functor<int, int, int> fn{&Add};
        larvae::AssertEqual(fn(3, 4), 7);
    });

    auto t_free_fn_void = larvae::RegisterTest("HiveFunctor", "FreeFunctionVoidReturn", []() {
        int x{0};
        hive::Functor<void, int&> fn{&Increment};
        fn(x);
        larvae::AssertEqual(x, 1);
    });

    // Member functions

    auto t_member_fn = larvae::RegisterTest("HiveFunctor", "MemberFunctionCall", []() {
        Widget w{10};
        hive::Functor<void, int> fn{&w, &Widget::Set};
        fn(42);
        larvae::AssertEqual(w.m_value, 42);
    });

    auto t_member_fn_return = larvae::RegisterTest("HiveFunctor", "MemberFunctionWithReturn", []() {
        Widget w{5};
        hive::Functor<int, int> fn{&w, &Widget::Multiply};
        int result = fn(3);
        larvae::AssertEqual(result, 15);
        larvae::AssertEqual(w.m_value, 15);
    });

    // Const member functions

    auto t_const_member = larvae::RegisterTest("HiveFunctor", "ConstMemberFunctionCall", []() {
        Widget w{99};
        hive::Functor<int> fn{&w, &Widget::Get};
        larvae::AssertEqual(fn(), 99);
    });

    // Default-constructed (empty)

    auto t_default = larvae::RegisterTest("HiveFunctor", "DefaultConstructedIsEmpty", []() {
        hive::Functor<void> fn;
        larvae::AssertTrue(fn.IsEmpty());
    });

    auto t_bound_not_empty = larvae::RegisterTest("HiveFunctor", "BoundFunctorIsNotEmpty", []() {
        hive::Functor<int, int, int> fn{&Add};
        larvae::AssertFalse(fn.IsEmpty());
    });

    // Copy

    auto t_copy_construct = larvae::RegisterTest("HiveFunctor", "CopyConstruct", []() {
        hive::Functor<int, int, int> fn{&Add};
        hive::Functor<int, int, int> copy{fn};
        larvae::AssertFalse(copy.IsEmpty());
        larvae::AssertEqual(copy(2, 3), 5);
    });

    auto t_copy_assign = larvae::RegisterTest("HiveFunctor", "CopyAssign", []() {
        hive::Functor<int, int, int> fn{&Add};
        hive::Functor<int, int, int> other;
        larvae::AssertTrue(other.IsEmpty());

        other = fn;
        larvae::AssertFalse(other.IsEmpty());
        larvae::AssertEqual(other(10, 20), 30);
    });

    auto t_copy_assign_empty = larvae::RegisterTest("HiveFunctor", "CopyAssignFromEmpty", []() {
        hive::Functor<int, int, int> fn{&Add};
        hive::Functor<int, int, int> empty;

        fn = empty;
        larvae::AssertTrue(fn.IsEmpty());
    });

} // namespace
