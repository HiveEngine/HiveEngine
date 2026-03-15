#pragma once

#include <larvae/assertions.h>
#include <larvae/benchmark.h>
#include <larvae/capabilities.h>
#include <larvae/benchmark_registry.h>
#include <larvae/benchmark_runner.h>
#include <larvae/fixture.h>
#include <larvae/test_registry.h>
#include <larvae/test_runner.h>

namespace larvae
{
    int RunAllTests(int argc, char** argv);
    int RunAllBenchmarks(int argc, char** argv);
} // namespace larvae
