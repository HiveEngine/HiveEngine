# HiveEngine Threading System

## Architecture Overview

HiveEngine uses a **unified job system** provided by the **Drone** module. All parallel
work in the engine flows through a single pool of worker threads — no module creates its
own `std::thread` instances.

```
Hive (core) → Comb (allocators) → Wax (containers) → Drone (jobs) → Queen / Nectar / Swarm
```

### Module Responsibilities

| Module | Role |
|--------|------|
| **Drone** | Job system, counters, coroutine primitives, per-worker scratch allocators |
| **Queen** | ECS parallel scheduler — submits system execution jobs to Drone |
| **Nectar** | Asset IO and cooking — submits IO and cook jobs to Drone |
| **Terra** | Windowing — single-threaded, main thread only |
| **Swarm** | Vulkan RHI — single-threaded (future: render thread via Drone) |

## Core Primitives

### JobSystem\<Allocator\>

Work-stealing thread pool with priority queues.

```cpp
#include <drone/job_system.h>

comb::BuddyAllocator alloc{8_MB};
drone::JobSystem<comb::BuddyAllocator> jobSystem{alloc, {
    .m_workerCount = 0,        // 0 = hardware_concurrency - 1
    .m_dequeCapacity = 4096,
    .m_globalCapacity = 4096,
    .m_scratchSize = 2_MB,     // per-worker scratch allocator
}};
jobSystem.Start();
```

**Worker count**: N-1 by default (leaves one core for the main thread / OS).

**Work stealing order** (per worker, each iteration):
1. Pop from local deque (LIFO — cache-hot subtasks)
2. Steal from global HIGH queue
3. Steal from global NORMAL queue
4. Steal from global LOW queue
5. Steal from a random victim worker's deque
6. Idle backoff: spin (64 iters) → yield (64 iters) → park (condition_variable, 500µs)

### Priority

```cpp
enum class Priority : uint8_t {
    HIGH   = 0,  // ECS systems, input processing
    NORMAL = 1,  // Asset cooking, general compute
    LOW    = 2,  // IO reads, garbage collection
};
```

### JobDecl

Type-erased job: function pointer + void* user data. No heap allocation.

```cpp
drone::JobDecl job;
job.m_func = [](void* data) { /* work */ };
job.m_userData = &myData;
job.m_priority = drone::Priority::HIGH;
```

### Counter

Atomic synchronization primitive. Replaces WaitGroup with C++20 `atomic::wait/notify`
(futex on Linux, WaitOnAddress on Windows — zero CPU waste while waiting).

```cpp
drone::Counter counter;
jobSystem.Submit(jobs, count, counter);  // counter incremented by count
counter.Wait();                          // blocks efficiently until counter == 0
```

### JobSubmitter

Type-erased interface for submitting jobs without knowing the JobSystem's allocator type.
Used by Nectar and other non-templated modules.

```cpp
drone::JobSubmitter submitter = drone::MakeJobSubmitter(jobSystem);
// Pass submitter to Nectar, Forge, etc. — they submit jobs without template coupling
```

### ParallelFor

Divides a range into chunks and distributes across workers.

```cpp
jobSystem.ParallelFor(0, entityCount,
    [](size_t i, void* data) {
        auto* transforms = static_cast<Transform*>(data);
        transforms[i].Update();
    },
    transformArray);
```

## Per-Worker Scratch Allocators

Each worker thread owns a `comb::LinearAllocator` (O(1) bump allocation, O(1) reset).
Use it for temporary per-frame data that doesn't outlive the frame.

```cpp
auto& scratch = jobSystem.WorkerScratch();  // thread-local lookup
void* temp = scratch.Allocate(sizeof(MyData), alignof(MyData));
// ... use temp ...
// No need to Deallocate — ResetAllScratch() at frame end frees everything
```

Call `jobSystem.ResetAllScratch()` once per frame after all jobs complete.

## C++20 Coroutines

Drone provides coroutine primitives for async workflows. All coroutine frames are
allocated from a global thread-safe slab allocator (size classes: 64, 128, 256, 512,
1024 bytes — O(1) alloc/dealloc, no fragmentation).

### Task\<T\>

Lazy coroutine that starts suspended and executes when co_awaited.
Uses symmetric transfer to avoid stack overflow on deep chains.

```cpp
#include <drone/task.h>

drone::Task<int> ComputeAsync() {
    co_return 42;
}

drone::Task<void> Caller() {
    int val = co_await ComputeAsync();
}
```

### ScheduleOn

Awaitable that moves execution to a Drone worker thread.

```cpp
#include <drone/schedule_on.h>

drone::Task<Result> ProcessAsync(drone::JobSubmitter& jobs) {
    // Running on caller's thread here
    co_await drone::ScheduleOn(jobs, drone::Priority::NORMAL);
    // Now running on a Drone worker
    co_return DoHeavyWork();
}
```

### AwaitCounter

Awaitable that suspends a coroutine until a Counter reaches zero.

```cpp
#include <drone/await_counter.h>

drone::Task<void> WaitForJobs(drone::JobSubmitter& jobs) {
    drone::Counter counter;
    jobs.Submit(jobBatch, batchSize, counter);
    co_await drone::AwaitCounter(jobs, counter);
    // All jobs complete
}
```

### ReadAsync (Nectar IOScheduler)

Coroutine-based file read that schedules IO on a worker and suspends the caller.

```cpp
drone::Task<wax::ByteBuffer> LoadMesh(nectar::IOScheduler& io) {
    wax::ByteBuffer data = co_await io.ReadAsync("meshes/player.nmsh");
    co_return data;
}
```

### SyncWait

Bridge between coroutine and non-coroutine code. Blocks the calling thread
until the task completes. Only use at the boundary (e.g. main loop).

```cpp
auto task = LoadMesh(io);
wax::ByteBuffer data = drone::SyncWait(task);
```

### Coroutine Frame Allocator

All `drone::Task<T>` coroutine frames are allocated from a global thread-safe slab
allocator (`drone::GetCoroutineAllocator()`). This avoids hitting the global heap
on every `co_await` chain.

- Size classes: 64, 128, 256, 512, 1024 bytes
- 4096 objects per size class
- Thread-safe via mutex wrapper
- Allocation/deallocation: O(1)

## Lock-Free Data Structures

### WorkStealingDeque (Chase-Lev)

Per-worker double-ended queue. Owner pushes/pops from bottom (LIFO), thieves steal
from top (FIFO). Lock-free using atomic CAS operations.

Reference: Le et al., "Correct and Efficient Work-Stealing for Weak Memory Models" (PPoPP 2013).

### MPMCQueue (Vyukov)

Bounded multi-producer multi-consumer queue used for global job submission.
Wait-free in the uncontended case. Cache-line aligned head/tail to prevent false sharing.

## Thread Safety Contracts

| Type | Thread-Safe | Notes |
|------|-------------|-------|
| `JobSystem` | Submit: yes, Start/Stop: main thread only | |
| `Counter` | All operations | Lock-free atomics |
| `JobSubmitter` | All operations | Delegates to JobSystem |
| `WorkStealingDeque` | Push/Pop: owner only, Steal: any thread | |
| `MPMCQueue` | All operations | Lock-free |
| `LinearAllocator` (scratch) | **No** — one per worker, no sharing | |
| `CoroutineAllocator` | Yes | Mutex-wrapped slab |
| `Task<T>` | **No** — single owner, move-only | |

## Rules for Engine Code

1. **Never create `std::thread` directly.** Submit jobs to Drone instead.
2. **Never share a `LinearAllocator` across threads.** Use `WorkerScratch()` for the
   current worker's allocator.
3. **Declare component access** in ECS systems so the scheduler can parallelize safely.
4. **Use `Arc<T>`** (not `Rc<T>`) for data shared across threads.
5. **Use `QueryEachLocked()`** (not `QueryEach()`) when querying from parallel systems.
6. **Prefer `co_await ScheduleOn(jobs)`** over blocking a worker with synchronous IO.
7. **Keep jobs short.** Long-blocking jobs (>1ms) reduce pool throughput. For IO, use
   `ReadAsync` or submit as LOW priority.

## File Map

```
Drone/
├── include/drone/
│   ├── job_system.h           // JobSystem<Allocator> — work-stealing pool
│   ├── job_types.h            // JobDecl, Priority
│   ├── job_submitter.h        // Type-erased job submission interface
│   ├── counter.h              // Atomic counter with C++20 wait/notify
│   ├── work_stealing_deque.h  // Chase-Lev lock-free deque
│   ├── mpmc_queue.h           // Vyukov bounded MPMC queue
│   ├── worker_context.h       // Thread-local worker index
│   ├── task.h                 // Task<T> coroutine + SyncWait
│   ├── schedule_on.h          // ScheduleOn awaitable
│   ├── await_counter.h        // AwaitCounter awaitable
│   └── coroutine_allocator.h  // Global slab allocator for coroutine frames
└── src/drone/
    ├── drone_module.cpp        // Header compilation check
    └── coroutine_allocator.cpp // Slab allocator singleton
```
