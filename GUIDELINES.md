# HiveEngine Code Guidelines

Ground rules for working in this codebase. The source of truth for machine-enforced style is:

- [`.clang-format`](.clang-format)
- [`.clang-tidy`](.clang-tidy)

If the prose below and the tooling disagree, update the prose or the config so they match.

---

## Language And Compiler

C++20, compiled with Clang targeting MSVC on Windows.

- No RTTI: `-fno-rtti`
- No exceptions: `-fno-exceptions`
- Warnings are strict. Treat warnings as real issues even when `-Werror` is not enabled.

If an operation can fail, return a value, assert, or redesign. Do not throw.

---

## Module Layout

Each module follows the same layout:

```text
ModuleName/
|- include/modulename/    Public headers
|  |- core/               Core types
|  |- storage/            Data structures
|  `- detail/             Internal details, not public API
|- src/modulename/        Implementation files
|- tests/                 test_*.cpp files
`- CMakeLists.txt
```

Bee-themed modules:

- `Queen`: ECS
- `Comb`: allocators
- `Wax`: containers, pointers, serialization
- `Drone`: job system, threading, coroutines
- `Larvae`: test framework
- `Hive`: core utilities
- `Nectar`: assets
- `Terra`: graphics and platform
- `Swarm`: rendering
- `Waggle`: runtime
- `Brood`: app targets
- `Forge`: editor UI

Namespaces follow `camelBack`. Single-word namespaces such as `queen`, `comb`, and `hive` are valid under this rule.

---

## Naming

The naming rules below are aligned to `.clang-tidy`.

| What                             | Style                                               | Examples                                      |
|----------------------------------|-----------------------------------------------------|-----------------------------------------------|
| Classes, structs, enums, aliases | `PascalCase`                                        | `Entity`, `LinearAllocator`, `ProjectConfig`  |
| Functions, methods               | `PascalCase`                                        | `Allocate()`, `IsAlive()`, `PushBack()`       |
| Template parameters              | `PascalCase`                                        | `T`, `Allocator`, `ComponentT`                |
| Namespaces                       | `camelBack`                                         | `queen`, `comb`, `projectTools`               |
| Enum constants                   | `UPPER_SNAKE`                                       | `ALLOCATE`, `INVALID`, `PLAYING`              |
| Macros, feature flags            | `UPPER_SNAKE`                                       | `HIVE_FEATURE_ASSERTS`, `COMB_MEM_DEBUG`      |
| Global constants                 | `UPPER_SNAKE`                                       | `MAX_ENTITIES`, `DEFAULT_TIMEOUT_MS`          |
| Static constants                 | `UPPER_SNAKE`                                       | `MAX_LEVELS`, `MIN_BLOCK_SIZE`                |
| Global variables                 | `camelBack` with `g_`                               | `g_moduleRegistry`, `g_jobSystem`             |
| Static variables                 | `camelBack` with `s_`                               | `s_cachedState`, `s_instanceCount`            |
| Member variables                 | `camelBack` with `m_`                               | `m_size`, `m_capacity`, `m_allocator`         |
| Parameters                       | `camelBack`                                         | `entityCount`, `allocator`, `projectPath`     |
| Local variables                  | `camelBack`                                         | `entityCount`, `oldCapacity`, `renderContext` |
| Local constants                  | `camelBack`                                         | `windowFlags`, `branchLimit`, `maxDepth`      |
| `constexpr` variables            | `camelBack` unless they are static/global constants | `chunkSize`, `headerMask`                     |
| Files                            | `snake_case`                                        | `entity_allocator.h`, `linear_allocator.cpp`  |
| Test files                       | `test_*.cpp`                                        | `test_entity.cpp`, `test_vector.cpp`          |

Additional naming rules:

- Predicates should read like predicates: `Is*`, `Has*`, `Can*`, `Contains*`.
- Prefer nouns for types and verbs for functions.
- Keep abbreviations consistent inside a module.

---

## Formatting

- 4-space indentation, no tabs.
- `#pragma once`, always.
- Classes, structs, enums, namespaces, functions, and methods use Allman braces.
- Control statements always use braces. Do not rely on single-line implicit blocks.

Example:

```cpp
namespace queen
{
    class Entity
    {
    public:
        [[nodiscard]] constexpr bool IsAlive() const noexcept
        {
            return HasFlag(Flags::ALIVE);
        }

        constexpr void SetFlag(Flags flag) noexcept
        {
            m_flags |= flag;
        }

    private:
        IndexType m_index{};
        GenerationType m_generation{};
        FlagsType m_flags{};
    };
}
```

Control flow:

```cpp
if (ptr == nullptr)
{
    return;
}
```

---

## Include Order

Use grouped include blocks with one blank line between blocks.

For `.cpp` files:

1. Own header first
2. Engine headers grouped by module block
3. Third-party headers
4. Standard library headers

Module block order is enforced by `.clang-format`:

1. `hive`
2. `comb`
3. `wax`
4. `drone`
5. `queen`
5. `nectar`
6. `waggle`
7. `swarm`
8. `terra`
9. `forge`
10. `antennae`
11. `larvae`
12. Third-party
13. Standard library

---

## Function Size And Complexity

The default thresholds enforced by `.clang-tidy` are:

- Cognitive complexity: 25
- Line count: 80
- Statement count: 50
- Branch count: 10
- Parameter count: 6
- Nesting depth: 4
- Local variable count: 20

These are warning thresholds, not excuses to write to the limit. Split functions when the logic stops being obvious.

---

## Initialization And Modernization

Prefer brace initialization:

```cpp
Entity entity{42, 7, Flags::ALIVE};
auto values = wax::Vector<int>{allocator};
return Entity{};
```

Prefer default member initialization:

```cpp
class Buffer
{
private:
    void* m_data{nullptr};
    size_t m_size{0};
};
```

Other modernization rules:

- Prefer `nullptr` over `NULL`.
- Prefer `emplace` where it is clearer than `push_back`.
- Do not force trailing return types unless they improve readability.
- Prefer range-based loops when they are clearer and safe.

---

## Const Correctness

Const correctness is part of the style, not an optional cleanup pass.

- Mark methods `const` when they do not mutate state.
- Use `constexpr` where it materially improves correctness or compile-time evaluation.
- Use `[[nodiscard]]` on return values that should not be ignored.
- Apply `const` to references and pointers when ownership and mutability permit it.

Typical signature:

```cpp
[[nodiscard]] constexpr bool IsEmpty() const noexcept
{
    return m_size == 0;
}
```

---

## Standard Library Policy

Engine runtime code (Queen, Nectar, Waggle, Terra, Swarm, Antennae) must use `wax::` types
instead of standard library containers and strings:

| std type | wax replacement | Header |
|----------|-----------------|--------|
| `std::string` | `wax::String` | `<wax/containers/string.h>` |
| `std::string_view` | `wax::StringView` | `<wax/containers/string_view.h>` |
| `std::vector` | `wax::Vector` | `<wax/containers/vector.h>` |
| `std::array` | `wax::Array` | `<wax/containers/array.h>` |
| `std::unordered_map` | `wax::HashMap` | `<wax/containers/hash_map.h>` |
| `std::unordered_set` | `wax::HashSet` | `<wax/containers/hash_set.h>` |
| `std::unique_ptr` | `wax::Box` | `<wax/pointers/box.h>` |
| `std::shared_ptr` | `wax::Rc` (single-thread) / `wax::Arc` (multi-thread) | `<wax/pointers/rc.h>` / `<wax/pointers/arc.h>` |
| `std::span` | `wax::Span` | `<wax/containers/span.h>` |

Allowed exceptions:

- `std::string_view` in `constexpr` / `consteval` contexts.
- Transient `std::string` forced by external APIs (`std::filesystem`, tinyobj, cgltf).
- Types with no wax equivalent: `std::pair`, `std::tuple`, `std::optional`, `std::variant`.
- System primitives: `std::atomic`, `std::thread`, `std::mutex`.

Tool and editor modules (Forge, Brood, Larvae) are exempt since their allocations do not
occur during gameplay. Hive and Comb are also exempt since they sit below Wax in the
dependency chain and cannot depend on it.

---

## Memory

No raw `new` or `delete` in engine code. Allocate through `comb`.

```cpp
auto* object = comb::New<MyClass>(allocator, arg1, arg2);
comb::Delete(allocator, object);

auto* array = comb::NewArray<int>(allocator, 64);
comb::DeleteArray(allocator, array, 64);
```

Containers and owning utilities should use the project allocator model.

The allocator must outlive everything allocated from it.

Typical allocator usage:

| Allocator          | Lifetime        | Use case                              |
|--------------------|-----------------|---------------------------------------|
| `LinearAllocator`  | Frame or scope  | Scratch data, temp buffers            |
| `BuddyAllocator`   | Persistent      | Long-lived allocations                |
| `PoolAllocator`    | Persistent      | Fixed-size high churn objects         |
| `DefaultAllocator` | Global fallback | Only when ownership is truly implicit |

---

## Threading

All parallel work flows through the Drone module's `JobSystem`. See `Drone/THREADING.md`
for the full API reference.

Rules:

- **Never create `std::thread`** in engine runtime code. Submit jobs to Drone.
- **Never share an allocator across threads** unless it is a `ThreadSafeAllocator` or
  the per-worker scratch (`WorkerScratch()`).
- **Use `Arc<T>`** for cross-thread shared ownership. `Rc<T>` is single-threaded only.
- **Use `drone::Priority`** to classify work: `HIGH` for gameplay-critical (ECS systems),
  `NORMAL` for throughput work (cooking), `LOW` for background work (IO, GC).
- **Keep jobs short.** A job blocking for >1ms degrades pool throughput. Use `ReadAsync`
  or `ScheduleOn` with coroutines for IO instead.
- **Prefer `co_await`** over `Counter::Wait()` in async pipelines to avoid blocking workers.
- **Use `QueryEachLocked()`** when iterating ECS queries from parallel systems.
- **Coroutine frames** are allocated from a global slab allocator (`drone::GetCoroutineAllocator()`).
  Do not use `operator new` for coroutine-heavy code paths — use `drone::Task<T>`.

---

## Error Handling

No exceptions.

Use the assert hierarchy from `<hive/core/assert.h>`:

| Function                  | Evaluates in release | Checks in release | Use for                          |
|---------------------------|----------------------|-------------------|----------------------------------|
| `hive::Assert(expr, msg)` | No                   | No                | Invariants, dev-time assumptions |
| `hive::Verify(expr, msg)` | Yes                  | No                | Side-effectful checks            |
| `hive::Check(expr, msg)`  | Yes                  | Yes               | Critical runtime invariants      |
| `hive::Unreachable()`     | N/A                  | N/A               | Impossible paths                 |
| `hive::NotImplemented()`  | N/A                  | N/A               | Explicit placeholders            |

For recoverable failures, return values such as `bool`, `nullptr`, or result structs.

---

## Templates And Concepts

Use concepts or equivalent constraints to keep template diagnostics sane.

```cpp
template<typename T, comb::Allocator Allocator = comb::DefaultAllocator>
class Vector
{
    // ...
};
```

Use `if constexpr` for compile-time branching where it materially simplifies specialization paths.

---

## Comments

Comments explain **why**, never **what**. If a comment restates the code, delete it.

Allowed:
- Constraints, invariants, or contracts not obvious from the signature.
- Rationale behind a non-obvious choice.
- ASCII memory layout diagrams when they clarify a data structure.

Forbidden:
- Restating code: `m_size = 0; // reset size`
- Trivial doxygen: `/// Returns the size` on `Size()`.
- AI-style template docstrings (Performance characteristics / Limitations / Use cases / Example sections).
  Two or three `//` lines before a class are enough.
- Decorative separator lines (`// ═════`, `// ─────`, `// =====`). Use blank lines to separate sections.
- Trailing comments that restate the field name: `float m_dt; // delta time in seconds`
- Numbered step-by-step comments (`// 1. Pop`, `// 2. Add guard`).
- Identical comments repeated across multiple methods — document the behavior once at the class level.

---

## Common Patterns

Builder pattern:

```cpp
auto entity = world.Spawn()
    .With(Position{1.0f, 2.0f, 3.0f})
    .With(Velocity{0.1f, 0.0f, 0.0f})
    .Build();
```

Deleted copy for ownership types:

```cpp
Box(const Box&) = delete;
Box& operator=(const Box&) = delete;
```

Static assertions should sit close to the type or invariant they protect:

```cpp
static_assert(sizeof(Entity) == 8, "Entity must stay compact");
static_assert(std::is_trivially_copyable_v<Entity>);
```

---

## Tests

Framework: Larvae.

Tests are auto-registered lambdas in anonymous namespaces:

```cpp
#include <larvae/larvae.h>
#include <queen/core/entity.h>
#include <comb/linear_allocator.h>

namespace
{
    auto defaultIsNull = larvae::RegisterTest("QueenEntity", "DefaultIsNull", []() {
        queen::Entity entity;
        larvae::AssertTrue(entity.IsNull());
        larvae::AssertFalse(entity.IsAlive());
    });
}
```

Rules:

- Use a dedicated allocator in tests.
- Do not rely on globals unless the test is explicitly about globals.
- Suite names should be stable and descriptive.
- Test names should describe the behavior being verified.

---

## Tooling

Formatting:

```bash
clang-format -i <files>
```

Linting:

```bash
clang-tidy -p out/build/... <file.cpp>
```

The expected enforcement profile is:

- `bugprone-*`
- `cppcoreguidelines-*`
- `modernize-*`
- `performance-*`
- `readability-*`
- `misc-*`
- `clang-analyzer-*`

with the explicit exclusions documented in `.clang-tidy`.

---

## Build

```bash
cmake --preset llvm-windows-debug
cmake --build out/build/llvm-debug --target larvae_runner
out/build/llvm-debug/bin/Debug/larvae_runner.exe
```

Sanitizer presets, if available:

```bash
cmake --preset llvm-windows-ubsan
cmake --preset llvm-windows-asan
```
