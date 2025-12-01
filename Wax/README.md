# Wax - Game Engine Containers

Containers and smart pointers for HiveEngine. Forces all heap allocations through Comb allocators.

## Features

- Containers with explicit allocator control
- Smart pointers (Box, Rc, Handle)
- Zero-overhead reference wrappers (Ref, Ptr)
- No hidden allocations

**What Wax is NOT:**
- Not a full STL replacement
- Stack-based types like `std::optional<T>` don't allocate - use them directly

## Quick Start

### Containers

```cpp
#include <wax/containers/vector.h>
#include <wax/containers/string.h>
#include <wax/containers/hash_map.h>
#include <comb/linear_allocator.h>

comb::LinearAllocator alloc{1_MB};

// Vector
wax::Vector<int> numbers{alloc};
numbers.PushBack(42);
numbers.PushBack(99);

// String (SSO for ≤22 chars)
wax::String name{alloc, "Player"};
name.Append(" One");

// FixedString - stack only, no allocator
wax::FixedString<32> tag{"Enemy_Tank"};

// HashMap (Robin Hood, open addressing)
wax::HashMap<EntityID, Entity> entities{alloc, 1000};
entities.Insert(id, entity);

if (auto* e = entities.Find(id)) {
    e->Update();
}
```

### Smart Pointers

```cpp
#include <wax/pointers/ref.h>
#include <wax/pointers/ptr.h>
#include <wax/pointers/box.h>
#include <wax/pointers/rc.h>
#include <wax/pointers/handle.h>

// Ref<T> - non-null reference (zero overhead)
void ProcessEntity(wax::Ref<Entity> entity) {
    entity->Update();  // never null
}

// Ptr<T> - nullable pointer (zero overhead)
wax::Ptr<Entity> FindEntity(EntityID id);

// Box<T, A> - unique ownership, 16 bytes
auto camera = wax::MakeBox<Camera>(alloc, position);

// Rc<T, A> - shared ownership, NON-ATOMIC (10-50x faster than std::shared_ptr)
auto texture = wax::MakeRc<Texture>(alloc, "player.png");
wax::Rc copy = texture;  // ref count = 2

// Handle<T> - generational index (8 bytes, use-after-free detection)
wax::HandlePool<Entity> pool{alloc, 1000};
auto handle = pool.Create(args...);
if (Entity* e = pool.Get(handle)) {
    e->Update();
}
pool.Destroy(handle);
// pool.Get(handle) now returns nullptr
```

### Choosing an Allocator

| Use Case | Allocator |
|----------|-----------|
| Frame temps, parsing | `LinearAllocator` |
| Scoped, LIFO | `StackAllocator` |
| Fixed-size objects (ECS) | `PoolAllocator<T>` |
| Multiple types, recycling | `SlabAllocator` |
| General-purpose | `BuddyAllocator` |

## API

### STL Migration

| STL | Wax | Notes |
|-----|-----|-------|
| `std::vector<T>` | `wax::Vector<T, A>` | Needs allocator |
| `std::array<T, N>` | `wax::Array<T, N>` | Drop-in |
| `std::string` | `wax::String<A>` | SSO + allocator |
| `std::unordered_map` | `wax::HashMap<K, V, A>` | Robin Hood |
| `std::unique_ptr<T>` | `wax::Box<T, A>` | Needs allocator |
| `std::shared_ptr<T>` | `wax::Rc<T, A>` | Non-atomic |
| `std::optional<T>` | `std::optional<T>` | Use STL directly |
| `T*` | `wax::Ptr<T>` | Type-safe nullable |
| `T&` | `wax::Ref<T>` | Rebindable non-null |

### What to Keep from STL

Use STL directly for types that don't heap-allocate:

```cpp
#include <optional>

std::optional<int> FindIndex(const char* name) {
    if (found) return index;
    return std::nullopt;
}
```

Also use: `std::move`, `std::forward`, `std::is_same_v`, `std::conditional_t`, etc.

## Structure

```
Wax/
├── include/wax/
│   ├── containers/
│   │   ├── array.h
│   │   ├── vector.h
│   │   ├── string.h
│   │   ├── string_view.h
│   │   ├── fixed_string.h
│   │   ├── hash_map.h
│   │   └── hash_set.h
│   └── pointers/
│       ├── ref.h
│       ├── ptr.h
│       ├── box.h
│       ├── rc.h
│       └── handle.h
└── tests/
    ├── test_vector.cpp
    ├── test_string.cpp
    └── pointers/
        └── test_box.cpp
```

## Status

**Done:**
- [x] Array, Span
- [x] Vector
- [x] String, StringView, FixedString
- [x] HashMap, HashSet
- [x] Ref, Ptr, Box, Rc
- [x] Handle, HandlePool

**Planned:**
- [ ] Arc (atomic Rc for multi-threading)
- [ ] FlatMap, FlatSet (sorted vector)
- [ ] Sort, Find, BinarySearch algorithms

## FAQ

**Q: Do I replace ALL STL?**

No. Use Wax for containers that allocate (vector, string, map). Use STL for stack-based types (optional, pair, tuple) and compile-time utilities.

**Q: Why non-atomic Rc?**

Game engines are mostly single-threaded per system. Non-atomic ref counting is 10-50x faster than `std::shared_ptr`. Use `Arc<T>` (planned) if you need thread safety.

**Q: Why Ref<T> instead of T&?**

References can't be reassigned or stored in containers. `Ref<T>` is rebindable, storable, with zero overhead.

**Q: Multi-threading?**

Wax containers are not thread-safe. Use per-thread allocators:

```cpp
thread_local comb::LinearAllocator t_alloc{10_MB};

void WorkerJob() {
    wax::Vector<Entity> local{t_alloc};
    // ... work
    t_alloc.Reset();
}
```
