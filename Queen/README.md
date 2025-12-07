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
- WorldAllocators - Integrated multi-allocator memory management

## Architecture

```
Queen/
├── include/queen/
│   ├── core/
│   │   ├── type_id.h        # Compile-time type hashing
│   │   ├── entity.h         # Entity ID (index + generation + flags)
│   │   └── component_info.h # Component metadata (type-erased lifecycle)
│   ├── storage/
│   │   ├── sparse_set.h     # Sparse set container
│   │   ├── column.h         # Type-erased component array
│   │   ├── table.h          # Archetype storage
│   │   └── archetype.h      # Archetype definition
│   ├── world/
│   │   └── world.h          # Main ECS world
│   ├── query/
│   │   └── query.h          # Query system
│   ├── command/
│   │   ├── command_buffer.h # Deferred mutations (CommandBuffer)
│   │   └── commands.h       # Thread-local command buffers
│   ├── system/
│   │   ├── system_id.h      # Type-safe system identifier
│   │   ├── access_descriptor.h # Read/write tracking
│   │   ├── system.h         # SystemDescriptor
│   │   ├── system_builder.h # Fluent system registration
│   │   ├── system_storage.h # System registry
│   │   └── resource_param.h # Res<T>/ResMut<T> wrappers
│   ├── core/
│   │   └── tick.h           # Tick and ComponentTicks for change detection
│   ├── query/
│   │   ├── change_filter.h  # Added<T>/Changed<T> filters
│   │   └── mut.h            # Mut<T> wrapper with change tracking
│   └── scheduler/
│       ├── system_node.h       # Dependency graph node
│       ├── dependency_graph.h  # Dependency graph with topological sort
│       ├── scheduler.h         # Sequential scheduler
│       ├── work_stealing_deque.h # Chase-Lev lock-free deque
│       ├── thread_pool.h       # Work-stealing thread pool
│       ├── parallel.h          # WaitGroup, parallel_for, TaskBatch
│       └── parallel_scheduler.h # Parallel system execution
├── src/queen/
│   └── ...
└── tests/
    └── test_*.cpp
```

## Quick Start

```cpp
#include <queen/world/world.h>


// Define components
struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };

// Define resources (global singletons)
struct Time { float elapsed, delta; };

int main() {
    // World manages its own memory via WorldAllocators
    queen::World world{};  // Manages its own memory

    // Insert resources
    world.InsertResource(Time{0.0f, 0.016f});

    // Spawn entities with variadic template
    auto player = world.Spawn(Position{0, 0, 0}, Velocity{1, 0, 0});

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

### Deferred Mutations with CommandBuffer

```cpp
#include <queen/command/command_buffer.h>

// During iteration - cannot modify World directly
queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

world.Query<queen::Read<Health>>().EachWithEntity([&](Entity e, const Health& hp) {
    if (hp.value <= 0) {
        cmd.Despawn(e);  // Deferred until Flush()
    }
});

// Apply all deferred commands atomically
cmd.Flush(world);

// Spawn with components via CommandBuffer
auto builder = cmd.Spawn()
    .With(Position{0, 0, 0})
    .With(Velocity{1, 0, 0});

cmd.Flush(world);

// Get the real entity after flush
Entity spawned = cmd.GetSpawnedEntity(builder.GetSpawnIndex());
```

### Systems

```cpp
#include <queen/world/world.h>

// Register systems with fluent API
queen::SystemId movement = world.System<queen::Read<Velocity>, queen::Write<Position>>("Movement")
    .Each([](const Velocity& vel, Position& pos) {
        pos.x += vel.dx;
        pos.y += vel.dy;
        pos.z += vel.dz;
    });

// System with entity access and deferred commands
queen::SystemId cleanup = world.System<queen::Read<Health>>("Cleanup")
    .EachWithCommands([](queen::Entity e, const Health& hp, queen::Commands<queen::PersistentAllocator>& cmd) {
        if (hp.value <= 0) {
            cmd.Get().Despawn(e);  // Deferred until after Update()
        }
    });

// System with Res<T> for read-only resource access
world.System<queen::Read<Position>>("Movement")
    .EachWithRes<Time>([](queen::Entity e, const Position& pos, queen::Res<Time> time) {
        // Access time->delta, time->elapsed (read-only)
    });

// System with ResMut<T> for mutable resource access
world.System("UpdateTime")
    .RunWithResMut<Time>([](queen::ResMut<Time> time) {
        time->elapsed += time->delta;  // Modify resource
    });

// Run individual system
world.RunSystem(movement);

// Run all registered systems in order
world.RunAllSystems();

// Enable/disable systems
world.SetSystemEnabled(cleanup, false);

// Update sequentially (default)
world.Update();

// Update with parallel system execution
// Independent systems run concurrently on multiple threads
world.UpdateParallel();

// Specify worker count (0 = auto-detect based on hardware)
world.UpdateParallel(4);  // Use 4 worker threads
```

## Status

**Phase 0 - Infrastructure:**
- [x] TypeId (compile-time type hash)
- [x] ComponentMask (dynamic bitset)
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
- [x] WorldAllocators (Persistent, Components, Frame, ThreadFrame allocators)
- [x] World (non-templated, uses WorldAllocators internally)
- [x] Entity operations (Spawn, Despawn, IsAlive)
- [x] Component operations (Get, Has, Add, Remove, Set)
- [x] Resources (InsertResource, Resource, HasResource, RemoveResource)

**Phase 4 - Queries:**
- [x] Query terms (TermOperator, TermAccess, Read, Write, With, Without, Maybe)
- [x] Query matching (QueryDescriptor, MatchesArchetype, FindMatchingArchetypes)
- [x] Query iteration (Query, Each, EachWithEntity)
- [x] Query builder (World::Query<>)

**Phase 5 - Command Buffers:**
- [x] CommandBuffer (deferred structural mutations)
- [x] SpawnCommandBuilder (builder for pending entities)
- [x] Block-based memory allocation for command data
- [x] Spawn/Despawn/Add/Remove/Set commands
- [x] Flush (apply commands to World)

**Phase 6 - Systems:**
- [x] SystemId (type-safe identifier)
- [x] AccessDescriptor (reads/writes tracking for parallel scheduling)
- [x] SystemDescriptor (type-erased system metadata)
- [x] SystemBuilder (fluent API with Each, EachWithEntity, Run)
- [x] SystemStorage (system registry and execution)
- [x] World integration (System<>, RunSystem, RunAllSystems)
- [x] Resource access declarations (WithResource, WithResourceMut)
- [x] Res<T>/ResMut<T> (resource parameters for system callbacks)
- [x] EachWithRes/EachWithResMut (entity iteration with resource access)
- [x] RunWithRes/RunWithResMut (resource-only systems)

**Phase 7 - Scheduling:**
- [x] SystemNode (dependency graph node)
- [x] DependencyGraph (system dependencies with topological sort)
- [x] Scheduler (sequential scheduler with automatic dependency inference)
- [x] World::Update() (runs all systems in dependency order)
- [x] Conflict detection (read-read ok, read-write/write-write conflicts)
- [x] Cycle detection in dependency graph
- [x] Commands (thread-local command buffers for deferred mutations)
- [x] EachWithCommands (system callback with Commands parameter)
- [x] Automatic command buffer flush at sync points

**Phase 8 - Change Detection:**
- [x] Tick (wraparound-safe tick counter)
- [x] ComponentTicks (added_tick, changed_tick per component)
- [x] World tick (incremented each Update())
- [x] Column tick storage (ticks per component per entity)
- [x] Added<T>, Changed<T>, AddedOrChanged<T> query filters
- [x] Mut<T> wrapper with automatic change marking
- [x] last_run_tick per system (updated after execution)
- [x] Query iteration with change filters

**Phase 9 - Parallelism:**
- [x] WorkStealingDeque (Chase-Lev lock-free deque)
- [x] ThreadPool (work-stealing thread pool)
- [x] WaitGroup (synchronization primitive)
- [x] parallel_for / parallel_for_each (parallel range iteration)
- [x] TaskBatch (batch task submission with wait)
- [x] ParallelScheduler (executes independent systems in parallel)
- [x] World::UpdateParallel() (parallel system execution integrated into World)

**Phase 10 - Events & Observers:**
- [ ] Event concept (trivially copyable event types)
- [ ] EventQueue (double-buffered event storage)
- [ ] EventWriter<T> (send events from systems)
- [ ] EventReader<T> (consume events in systems)
- [ ] Events registry (type-erased event queue storage)
- [ ] OnAdd<T>, OnRemove<T>, OnSet<T> (observer trigger types)
- [ ] Observer (reactive callback on structural changes)
- [ ] ObserverBuilder (fluent API for observer registration)
- [ ] ObserverStorage (observer registry)
- [ ] World::Events<T>() / World::SendEvent<T>()
- [ ] World::Observer<T>() builder
- [ ] System integration (EventWriter/EventReader as parameters)
