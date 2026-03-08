#include <larvae/larvae.h>

#include <brood/process_runtime.h>

int main(int argc, char** argv)
{
    brood::ProcessRuntime runtime{};
    const int result = larvae::RunAllTests(argc, argv);
    runtime.Finalize();
    return result;
}
