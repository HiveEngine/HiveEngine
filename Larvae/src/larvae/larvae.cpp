#include <larvae/larvae.h>
#include <larvae/precomp.h>

#include <iostream>

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

        if (config.list_only)
        {
            return config.fail_on_skip && runner.GetSkippedBenchmarks() > 0 ? 1 : 0;
        }

        PrintBenchmarkResults(results);
        if (runner.GetSkippedBenchmarks() > 0)
        {
            std::cout << "Skipped " << runner.GetSkippedBenchmarks()
                      << " benchmark(s) due to missing capabilities.\n";
        }

        if (runner.GetMatchedBenchmarks() == 0)
        {
            return 1;
        }

        if (config.fail_on_skip && runner.GetSkippedBenchmarks() > 0)
        {
            return 1;
        }

        return results.empty() ? 1 : 0;
    }
} // namespace larvae
