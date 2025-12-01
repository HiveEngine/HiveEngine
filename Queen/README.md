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
#include <queen/queen.h>
#include <comb/linear_allocator.h>

// Define components
struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Player {};  // Tag component

int main() {
    comb::LinearAllocator alloc{100_MB};
    queen::World world{alloc};

    // Spawn entities
    auto player = world.Spawn()
        .With(Position{0, 0, 0})
        .With(Velocity{1, 0, 0})
        .With<Player>()
        .Build();

    // Query and iterate
    world.Query<Position, Velocity>().Each([](Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
        pos.z += vel.dz;
    });

    // Despawn
    world.Despawn(player);
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
- [ ] ComponentInfo
- [ ] Column
- [ ] Table
- [ ] Archetype
- [ ] ArchetypeGraph

**Phase 3 - World:**
- [ ] World
- [ ] Entity operations
- [ ] Component operations
- [ ] Resources

**Phase 4 - Queries:**
- [ ] Query terms
- [ ] Query matching
- [ ] Query iteration
