# Larvae - Testing Framework

Unit testing framework for HiveEngine. Lightweight, no external dependencies.

## Features

- Auto-registration of tests
- Fixtures support (setup/teardown)
- Pattern/wildcard filtering
- Built-in benchmarks
- Cross-platform (Windows, Linux, macOS)

## Quick Start

### Simple Test

```cpp
#include <larvae/larvae.h>

static auto test1 = larvae::RegisterTest("MathSuite", "AdditionWorks", []() {
    int result = 2 + 3;
    larvae::AssertEqual(result, 5);
});
```

### Test with Fixture

```cpp
class AllocatorFixture : public larvae::TestFixture
{
protected:
    void SetUp() override
    {
        allocator = new MyAllocator{1024};
    }

    void TearDown() override
    {
        delete allocator;
    }

    MyAllocator* allocator = nullptr;
};

static auto test2 = larvae::RegisterTestWithFixture<AllocatorFixture>(
    "AllocatorSuite", "AllocateWorks",
    [](AllocatorFixture& fixture) {
        void* ptr = fixture.allocator->Allocate(64, 8);
        larvae::AssertNotNull(ptr);
    });
```

### Running Tests

```bash
cmake -B build
cmake --build build

# Run all tests
./build/bin/larvae_runner

# Filter by suite
./build/bin/larvae_runner --suite=Comb

# Filter by pattern
./build/bin/larvae_runner --filter=*Allocator*

# Other options
./build/bin/larvae_runner --verbose
./build/bin/larvae_runner --repeat=10
./build/bin/larvae_runner --shuffle
./build/bin/larvae_runner --stop-on-failure
```

## API

### Assertions

```cpp
// Equality
larvae::AssertEqual(actual, expected)
larvae::AssertNotEqual(actual, expected)

// Comparisons
larvae::AssertLessThan(a, b)
larvae::AssertLessEqual(a, b)
larvae::AssertGreaterThan(a, b)
larvae::AssertGreaterEqual(a, b)

// Booleans
larvae::AssertTrue(condition)
larvae::AssertFalse(condition)

// Pointers
larvae::AssertNull(ptr)
larvae::AssertNotNull(ptr)

// Floats
larvae::AssertNear(a, b, epsilon)
larvae::AssertFloatEqual(a, b)      // epsilon = 1e-5f
larvae::AssertDoubleEqual(a, b)     // epsilon = 1e-9

// Strings
larvae::AssertStringEqual(str1, str2)
larvae::AssertStringNotEqual(str1, str2)
```

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | All tests pass |
| `1` | At least one test fails |

## Structure

```
Larvae/
├── include/larvae/
│   ├── larvae.h
│   ├── test_registry.h
│   ├── test_runner.h
│   ├── assertions.h
│   ├── fixture.h
│   ├── test_info.h
│   └── test_result.h
└── src/larvae/
    ├── test_registry.cpp
    ├── test_runner.cpp
    ├── assertions.cpp
    └── larvae.cpp

Brood/
├── main.cpp
└── CMakeLists.txt
```

## Adding Tests

1. Create a file `test_*.cpp` in module's `tests/` directory
2. Include `<larvae/larvae.h>`
3. Use `RegisterTest()` or `RegisterTestWithFixture<>()`

```cpp
// Comb/tests/test_myclass.cpp
#include <larvae/larvae.h>
#include <mymodule/myclass.h>

static auto test_basic = larvae::RegisterTest("MyModule", "BasicTest", []() {
    MyClass obj;
    larvae::AssertTrue(obj.IsValid());
});
```
