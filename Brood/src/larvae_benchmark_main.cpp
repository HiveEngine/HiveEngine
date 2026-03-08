#include <larvae/larvae.h>

#include <brood/process_runtime.h>
#include <iostream>

int main(int argc, char** argv)
{
    brood::ProcessRuntime runtime{};

    std::cout << "Larvae Benchmark Runner\n\n";
    const int result = larvae::RunAllBenchmarks(argc, argv);
    runtime.Finalize();
    return result;
}
