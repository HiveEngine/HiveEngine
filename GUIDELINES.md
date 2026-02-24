# HiveEngine — Code Guidelines

Ground rules for working in this codebase. Not a style bible, just what you need to know so things stay consistent.

---

## Language & Compiler

C++20, compiled with Clang targeting MSVC on Windows. No RTTI (`-fno-rtti`), no exceptions (`-fno-exceptions`). If you need to handle an error, don't throw — return a value, use an assert, or redesign.

Strict warnings are on (`-Wall -Wextra -Wpedantic -Wconversion` and friends). Treat warnings seriously even if `-Werror` isn't always enabled.

---

## Module Layout

Each module follows the same structure:

```
ModuleName/
├── include/modulename/    Public headers
│   ├── core/              Core types
│   ├── storage/           Data structures (if any)
│   └── detail/            Internal stuff, not part of public API
├── src/modulename/        Implementation files
├── tests/                 test_*.cpp files (auto-discovered by CMake)
└── CMakeLists.txt
```

Modules are bee-themed: **Queen** (ECS), **Comb** (allocators), **Wax** (containers, pointers, serialization), **Larvae** (test framework), **Hive** (core utilities), **Nectar** (assets), **Terra** (graphics), **Brood** (build target).

Each module has its own namespace in `snake_case`: `queen::`, `comb::`, `wax::`, etc. Internal helpers go in a `detail` sub-namespace.

---

## Naming

| What | Style | Examples |
|---|---|---|
| Classes, structs | `PascalCase` | `Entity`, `LinearAllocator`, `SparseSet` |
| Functions, methods | `PascalCase` | `Allocate()`, `IsAlive()`, `PushBack()` |
| Member variables | `trailing_underscore_` | `data_`, `size_`, `allocator_` |
| Local variables | `snake_case` | `entity_count`, `old_capacity` |
| Namespaces | `snake_case` | `queen`, `comb::detail` |
| Template params | `PascalCase` | `T`, `Allocator`, `ComponentT` |
| Compile-time constants | `kPascalCase` | `kMaxIndex`, `kAlive`, `kSsoCapacity` |
| Macros, feature flags | `UPPER_SNAKE` | `HIVE_FEATURE_ASSERTS`, `HIVE_PLATFORM_WINDOWS` |
| Files | `snake_case` | `entity_allocator.h`, `linear_allocator.cpp` |
| Test files | `test_*.cpp` | `test_entity.cpp`, `test_vector.cpp` |

Predicates: `Is*`, `Has*`, `Contains*`. Not `Check*` or `ShouldBe*`.

---

## Formatting

- 4-space indentation, no tabs.
- `#pragma once`, always.
- Allman braces for classes/structs/namespaces, same-line braces for functions/methods:

```cpp
namespace queen
{
    class Entity
    {
    public:
        [[nodiscard]] constexpr bool IsAlive() const noexcept {
            return HasFlag(Flags::kAlive);
        }

        constexpr void SetFlag(FlagsType flag) noexcept {
            flags_ |= flag;
        }

    private:
        IndexType index_;
        GenerationType generation_;
        FlagsType flags_;
    };
}
```

Short one-liners can stay on one line. Multi-line bodies get braces on their own lines for `if`/`for`/`while`:

```cpp
if (ptr == nullptr)
{
    return;
}
```

---

## Initialization

Prefer brace initialization `T{args...}` over parentheses `T(args...)` wherever possible:

```cpp
Entity e{42, 7, Flags::kAlive};          // good
auto vec = Vector<int, LinearAllocator>{alloc};  // good
return Entity{};                           // good
```

Member initializer lists: one member per line, comma-leading:

```cpp
Vector(Allocator& allocator) noexcept
    : data_{nullptr}
    , size_{0}
    , capacity_{0}
    , allocator_{&allocator}
{}
```

---

## Include Order

For `.cpp` files, the corresponding header comes first. Then engine modules (hive first, then others), then standard library:

```cpp
#include <modulename/my_class.h>  // 1. Own header (in .cpp files)

#include <hive/core/assert.h>     // 2. Engine modules (hive first, then others)
#include <comb/allocator_concepts.h>
#include <comb/default_allocator.h>
#include <wax/containers/vector.h>

#include <cstdint>                // 3. Standard library
#include <utility>
```

For headers, same order without the "own header" step.

No blank lines between groups — the order itself makes it clear.

---

## Attributes & Qualifiers

Use them. They're not optional decorations.

- **`[[nodiscard]]`** on anything that returns a value you shouldn't ignore: getters, allocations, queries, predicates.
- **`constexpr`** on anything that can run at compile time. Use `consteval` when it *must* run at compile time (type IDs, hashes).
- **`noexcept`** on trivial operations, move constructors, destructors, getters.
- **`const`** on methods that don't mutate state. Always. No excuses.

Typical signature:

```cpp
[[nodiscard]] constexpr bool IsEmpty() const noexcept {
    return size_ == 0;
}
```

---

## Memory

No raw `new`/`delete`. Ever. All allocations go through `comb` allocators.

```cpp
// Allocating objects
auto* obj = comb::New<MyClass>(allocator, arg1, arg2);
comb::Delete(allocator, obj);

// Arrays
auto* arr = comb::NewArray<int>(allocator, 64);
comb::DeleteArray(allocator, arr, 64);
```

Containers take an allocator as template param + constructor arg:

```cpp
wax::Vector<int, comb::LinearAllocator> vec{alloc};
wax::String<comb::BuddyAllocator> str{alloc, "hello"};
```

Smart pointers: `wax::Box<T>` (unique), `wax::Rc<T>` (refcounted). Create with `MakeBox`/`MakeRc`.

The allocator is passed by reference and stored as a pointer internally. **The allocator must outlive everything it allocated.** This is your responsibility — no safety net.

Allocator types and when to use them:

| Allocator | Lifetime | Use case |
|---|---|---|
| `LinearAllocator` | Frame/scope | Temp data, query results, scratch buffers |
| `BuddyAllocator` | Persistent | Long-lived data, archetypes, systems |
| `PoolAllocator` | Persistent | Fixed-size objects, high churn |
| `DefaultAllocator` | Global | When allocator ownership is truly implicit |

---

## Error Handling

No exceptions. The project compiles with `-fno-exceptions`.

Use the assert hierarchy from `<hive/core/assert.h>`:

| Function | Evaluates expr in release? | Checks in release? | Use for |
|---|---|---|---|
| `hive::Assert(expr, msg)` | No | No | Invariants, dev-time sanity checks |
| `hive::Verify(expr, msg)` | Yes | No | Side-effectful checks needed in release |
| `hive::Check(expr, msg)` | Yes | Yes | Critical invariants, use sparingly |
| `hive::Unreachable()` | — | — | Impossible code paths (switch defaults) |
| `hive::NotImplemented()` | — | — | Placeholder for unfinished work |

For recoverable failures, return values: `nullptr` on allocation failure, `bool` for operations that can fail, etc.

---

## Templates & Concepts

Use concepts to constrain allocator types:

```cpp
template<typename T, comb::Allocator Allocator = comb::DefaultAllocator>
class Vector { ... };
```

Use `if constexpr` for compile-time branching (trivially copyable optimizations, etc.):

```cpp
if constexpr (std::is_trivially_destructible_v<T>)
{
    // skip destructor calls
}
else
{
    for (size_t i = count; i > 0; --i)
        ptr[i - 1].~T();
}
```

Type erasure when needed: store `void*` + function pointers for destruction/move in a metadata struct. See `ComponentMeta` in Queen.

---

## Common Patterns

**Builder pattern** — chainable methods returning `*this`:

```cpp
auto entity = world.Spawn()
    .With(Position{1.0f, 2.0f, 3.0f})
    .With(Velocity{0.1f, 0.0f, 0.0f})
    .Build();
```

**Size vs Capacity** — all containers distinguish between `Size()` (element count) and `Capacity()` (allocated slots). Growth factor is 2x.

**Predicates** — `IsEmpty()`, `IsFull()`, `IsAlive()`, `IsValid()`, `Contains()`.

**Cleanup** — `Reset()` releases resources, `Clear()` removes elements but keeps memory.

**Deleted copies** — types with ownership semantics (`Box`, allocators) delete copy operations:

```cpp
Box(const Box&) = delete;
Box& operator=(const Box&) = delete;
```

**`static_assert` for invariants** — put them right after the class definition:

```cpp
static_assert(sizeof(Entity) == 8, "Entity must be 8 bytes");
static_assert(std::is_trivially_copyable_v<Entity>);
```

---

## Tests

Framework is Larvae. Tests are auto-registered lambdas in anonymous namespaces:

```cpp
#include <larvae/larvae.h>
#include <queen/core/entity.h>
#include <comb/linear_allocator.h>

namespace
{
    auto test1 = larvae::RegisterTest("QueenEntity", "DefaultIsNull", []() {
        queen::Entity e;
        larvae::AssertTrue(e.IsNull());
        larvae::AssertFalse(e.IsAlive());
    });

    auto test2 = larvae::RegisterTest("QueenEntity", "ConstructorStoresValues", []() {
        queen::Entity e{42, 7, queen::Entity::Flags::kAlive};

        larvae::AssertEqual(e.Index(), 42u);
        larvae::AssertEqual(e.Generation(), static_cast<uint16_t>(7));
        larvae::AssertTrue(e.IsAlive());
    });
}
```

Suite name = module + class being tested. Test name = what's being verified, in PascalCase.

Always use a dedicated allocator in tests, don't rely on globals. After adding a new `test_*.cpp` file, reconfigure CMake (the glob needs to pick it up).

Available assertions: `AssertEqual`, `AssertNotEqual`, `AssertTrue`, `AssertFalse`, `AssertNull`, `AssertNotNull`, `AssertLessThan`, `AssertGreaterThan`, `AssertNear`, `AssertFloatEqual`.

---

## Build

```bash
# Configure
cmake --preset llvm-windows-debug

# Build tests
cmake --build out/build/llvm-debug --target larvae_runner

# Run tests
out/build/llvm-debug/bin/Debug/larvae_runner.exe

# Sanitizers
cmake --preset llvm-windows-ubsan    # UBSan
cmake --preset llvm-windows-memdebug # ASan
```

---

## Things to Avoid

- `new`/`delete` — use `comb::New`/`comb::Delete`
- `std::vector`, `std::string` — use `wax::Vector`, `wax::String`
- Exceptions, `try`/`catch`
- RTTI, `dynamic_cast`, `typeid`
- `auto` when the type isn't immediately obvious
- Tab characters
- Trailing whitespace
- `#include` guards (use `#pragma once`)
