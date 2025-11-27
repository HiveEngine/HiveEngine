#pragma once

#include <larvae/test_registry.h>
#include <larvae/test_runner.h>
#include <larvae/assertions.h>
#include <larvae/fixture.h>
#include <larvae/benchmark.h>
#include <larvae/benchmark_registry.h>
#include <larvae/benchmark_runner.h>

namespace larvae
{
    int RunAllTests(int argc, char** argv);
    int RunAllBenchmarks(int argc, char** argv);
}
