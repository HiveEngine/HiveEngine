# Queen - ECS + Task Scheduler

Entity Component System and parallel task scheduler for HiveEngine.

## Features

- Archetype-based storage (cache-friendly iteration)
- Sparse storage for volatile components
- Compile-time type IDs (zero runtime overhead)
- Generational entity IDs (use-after-free detection)
- Query system with filters (With, Without, Maybe)
- Change detection
- Command buffers for deferred mutations
- Parallel task scheduler with work stealing

## Architecture

```
Queen/
├── include/queen/
│   ├── core/
│   │   ├── type_id.h        # Compile-time type hashing
│   │   ├── entity.h         # Entity ID (index + generation + flags)
│   │   └── component_mask.h # Dynamic bitset for component presence
│   ├── storage/
│   │   ├── sparse_set.h     # Sparse set container
│   │   ├── column.h         # Type-erased component array
│   │   ├── table.h          # Archetype storage
│   │   └── archetype.h      # Archetype definition
│   ├── world/
│   │   └── world.h          # Main ECS world
│   ├── query/
│   │   └── query.h          # Query system
│   └── scheduler/
│       └── scheduler.h      # Task scheduler
├── src/queen/
│   └── ...
└── tests/
    └── test_*.cpp
```

## Quick Start

```cpp
#include <queen/world/world.h>
#include <comb/linear_allocator.h>

// Define components
struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };

// Define resources (global singletons)
struct Time { float elapsed, delta; };

int main() {
    comb::LinearAllocator alloc{10_MB};
    queen::World<comb::LinearAllocator> world{alloc};

    // Insert resources
    world.InsertResource(Time{0.0f, 0.016f});

    // Spawn entities with builder pattern
    auto player = world.Spawn()
        .With(Position{0, 0, 0})
        .With(Velocity{1, 0, 0})
        .Build();

    // Or spawn with variadic template
    auto enemy = world.Spawn(Position{10, 0, 0}, Velocity{-1, 0, 0});

    // Access components
    Position* pos = world.Get<Position>(player);
    Velocity* vel = world.Get<Velocity>(player);

    // Access resources
    Time* time = world.Resource<Time>();
    pos->x += vel->dx * time->delta;

    // Add/remove components dynamically
    world.Add<Velocity>(player, Velocity{2, 0, 0});
    world.Remove<Velocity>(player);

    // Despawn
    world.Despawn(player);
    world.Despawn(enemy);
}
```

## Status

**Phase 0 - Infrastructure:**
- [x] TypeId (compile-time type hash)
- [ ] ComponentMask (dynamic bitset)
- [x] SparseSet<T>

**Phase 1 - Entities:**
- [x] Entity (64-bit ID)
- [x] EntityAllocator
- [x] EntityLocationMap

**Phase 2 - Storage:**
- [x] ComponentInfo
- [x] Column
- [x] Table
- [x] Archetype
- [x] ArchetypeGraph
- [x] ComponentIndex

**Phase 3 - World:**
- [x] World
- [x] Entity operations (Spawn, Despawn, IsAlive)
- [x] Component operations (Get, Has, Add, Remove, Set)
- [x] Resources (InsertResource, Resource, HasResource, RemoveResource)

**Phase 4 - Queries:**
- [x] Query terms (TermOperator, TermAccess, Read, Write, With, Without, Maybe)
- [x] Query matching (QueryDescriptor, MatchesArchetype, FindMatchingArchetypes)
- [x] Query iteration (Query, Each, EachWithEntity)
- [ ] Query builder (World::Query<>)
