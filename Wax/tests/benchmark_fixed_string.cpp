#include <larvae/larvae.h>
#include <wax/containers/fixed_string.h>
#include <wax/containers/string_view.h>
#include <string>

namespace
{
    // =============================================================================
    // Wax::FixedString Benchmarks
    // =============================================================================

    auto bench1 = larvae::RegisterBenchmark("WaxFixedString", "ConstructSmallString", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::FixedString str{"Hello"};
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench2 = larvae::RegisterBenchmark("WaxFixedString", "ConstructMaxCapacity", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::FixedString str{"1234567890123456789012"};
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench3 = larvae::RegisterBenchmark("WaxFixedString", "AppendChars", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::FixedString str;
            for (int i = 0; i < 20; ++i)
            {
                str.Append('a');
            }
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations() * 20);
    });

    auto bench4 = larvae::RegisterBenchmark("WaxFixedString", "AppendStrings", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::FixedString str;
            str.Append("Hi");
            str.Append(" ");
            str.Append("World");
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations() * 3);
    });

    auto bench5 = larvae::RegisterBenchmark("WaxFixedString", "FindChar", [](larvae::BenchmarkState& state) {
        wax::FixedString str{"The quick brown fox"};

        while (state.KeepRunning())
        {
            size_t pos = str.Find('x');
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench6 = larvae::RegisterBenchmark("WaxFixedString", "FindSubstring", [](larvae::BenchmarkState& state) {
        wax::FixedString str{"The quick brown fox"};

        while (state.KeepRunning())
        {
            size_t pos = str.Find("fox");
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench7 = larvae::RegisterBenchmark("WaxFixedString", "Compare", [](larvae::BenchmarkState& state) {
        wax::FixedString str1{"Hello World"};
        wax::FixedString str2{"Hello World"};

        while (state.KeepRunning())
        {
            bool result = str1 == str2;
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench8 = larvae::RegisterBenchmark("WaxFixedString", "Copy", [](larvae::BenchmarkState& state) {
        wax::FixedString source{"Hello World"};

        while (state.KeepRunning())
        {
            wax::FixedString copy{source};
            larvae::DoNotOptimize(copy.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench9 = larvae::RegisterBenchmark("WaxFixedString", "Resize", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::FixedString str{"Hi"};
            str.Resize(10, 'x');
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench10 = larvae::RegisterBenchmark("WaxFixedString", "Clear", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::FixedString str{"Hello World"};
            str.Clear();
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench11 = larvae::RegisterBenchmark("WaxFixedString", "ToStringView", [](larvae::BenchmarkState& state) {
        wax::FixedString str{"Hello World"};

        while (state.KeepRunning())
        {
            wax::StringView sv = str.View();
            larvae::DoNotOptimize(sv.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench12 = larvae::RegisterBenchmark("WaxFixedString", "Iteration", [](larvae::BenchmarkState& state) {
        wax::FixedString str{"Hello World"};

        while (state.KeepRunning())
        {
            size_t count = 0;
            for (char ch : str)
            {
                if (ch == ' ')
                {
                    ++count;
                }
            }
            larvae::DoNotOptimize(count);
        }

        state.SetItemsProcessed(state.iterations() * str.Size());
    });

    // =============================================================================
    // Comparison: FixedString vs std::string (small strings)
    // =============================================================================

    auto bench13 = larvae::RegisterBenchmark("StdStringSmall", "ConstructSmallString", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string str{"Hello"};
            larvae::DoNotOptimize(str.data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench14 = larvae::RegisterBenchmark("StdStringSmall", "AppendChars", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string str;
            for (int i = 0; i < 20; ++i)
            {
                str.push_back('a');
            }
            larvae::DoNotOptimize(str.data());
        }

        state.SetItemsProcessed(state.iterations() * 20);
    });

    auto bench15 = larvae::RegisterBenchmark("StdStringSmall", "Copy", [](larvae::BenchmarkState& state) {
        std::string source{"Hello World"};

        while (state.KeepRunning())
        {
            std::string copy{source};
            larvae::DoNotOptimize(copy.data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench16 = larvae::RegisterBenchmark("StdStringSmall", "Compare", [](larvae::BenchmarkState& state) {
        std::string str1{"Hello World"};
        std::string str2{"Hello World"};

        while (state.KeepRunning())
        {
            bool result = str1 == str2;
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });
}
