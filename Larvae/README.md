# Larvae

Unit testing framework for HiveEngine. Lightweight, no external dependencies (no Google Test, Catch2, etc.).

## Features

- Auto-registration of tests
- Fixtures support (setup/teardown)
- Pattern/wildcard filtering
- Built-in benchmarks
- Works on Windows, Linux, macOS

## Quick usage

### Simple test

```cpp
#include <larvae/larvae.h>

static auto test1 = larvae::RegisterTest("MathSuite", "AdditionWorks", []() {
    int result = 2 + 3;
    larvae::AssertEqual(result, 5);
});
```

### Test with fixture

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

### Available assertions

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

// Floats with epsilon
larvae::AssertNear(a, b, epsilon)
larvae::AssertFloatEqual(a, b)      // epsilon = 1e-5f
larvae::AssertDoubleEqual(a, b)     // epsilon = 1e-9

// Strings (string_view, const char*, std::string)
larvae::AssertStringEqual(str1, str2)
larvae::AssertStringNotEqual(str1, str2)
```

## Build and run

```bash
cmake -B build
cmake --build build

# Run all tests
./build/bin/larvae_runner

# Filter by suite
./build/bin/larvae_runner --suite=Comb

# Filter by pattern
./build/bin/larvae_runner --filter=*Allocator*

# Verbose mode
./build/bin/larvae_runner --verbose

# Repeat N times
./build/bin/larvae_runner --repeat=10

# Random order
./build/bin/larvae_runner --shuffle

# Stop on first failure
./build/bin/larvae_runner --stop-on-failure
```

## Structure

```
Larvae/
├── include/larvae/
│   ├── larvae.h           # Main header
│   ├── test_registry.h    # Test registration
│   ├── test_runner.h      # Execution
│   ├── assertions.h       # Assertions
│   ├── fixture.h          # Fixtures
│   ├── test_info.h
│   └── test_result.h
└── src/larvae/
    ├── test_registry.cpp
    ├── test_runner.cpp
    ├── assertions.cpp
    └── larvae.cpp

Brood/                     # The tests themselves
├── main.cpp
├── memory/
│   └── test_allocator.cpp
└── CMakeLists.txt
```

## Adding tests

1. Create a file in `Brood/module_name/`
2. Include `<larvae/larvae.h>`
3. Use `RegisterTest()` or `RegisterTestWithFixture<>()`
4. Add the file to `Brood/CMakeLists.txt`

```cpp
// Brood/mymodule/test_myclass.cpp
#include <larvae/larvae.h>
#include <mymodule/myclass.h>

static auto test_basic = larvae::RegisterTest("MyModule", "BasicTest", []() {
    MyClass obj;
    larvae::AssertTrue(obj.IsValid());
});
```

## Exit codes

- `0`: all tests pass
- `1`: at least one test fails

