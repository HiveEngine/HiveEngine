#include <larvae/precomp.h>
#include <larvae/larvae.h>

namespace larvae
{
    int RunAllTests(int argc, char** argv)
    {
        TestRunnerConfig config = ParseCommandLine(argc, argv);
        TestRunner runner{config};
        return runner.Run();
    }

    int RunAllBenchmarks(int argc, char** argv)
    {
        BenchmarkConfig config = ParseBenchmarkCommandLine(argc, argv);
        BenchmarkRunner runner{config};
        auto results = runner.RunAll();
        PrintBenchmarkResults(results);
        return results.empty() ? 1 : 0;
    }
}
