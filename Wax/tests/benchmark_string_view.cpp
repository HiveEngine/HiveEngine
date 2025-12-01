#include <larvae/larvae.h>
#include <wax/containers/string_view.h>
#include <string_view>

namespace
{
    // =============================================================================
    // Wax::StringView Benchmarks
    // =============================================================================

    auto bench1 = larvae::RegisterBenchmark("WaxStringView", "ConstructFromLiteral", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::StringView sv{"Hello World"};
            larvae::DoNotOptimize(sv.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench2 = larvae::RegisterBenchmark("WaxStringView", "ConstructFromPointer", [](larvae::BenchmarkState& state) {
        const char* str = "Hello World";

        while (state.KeepRunning())
        {
            wax::StringView sv{str, 11};
            larvae::DoNotOptimize(sv.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench3 = larvae::RegisterBenchmark("WaxStringView", "Iteration", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t count = 0;
            for (char ch : sv)
            {
                if (ch == ' ')
                {
                    ++count;
                }
            }
            larvae::DoNotOptimize(count);
        }

        state.SetItemsProcessed(state.iterations() * sv.Size());
    });

    auto bench4 = larvae::RegisterBenchmark("WaxStringView", "FindChar", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.Find('z');
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench5 = larvae::RegisterBenchmark("WaxStringView", "FindSubstring", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.Find("lazy");
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench6 = larvae::RegisterBenchmark("WaxStringView", "FindNotFound", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.Find("zebra");
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench7 = larvae::RegisterBenchmark("WaxStringView", "RFindChar", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.RFind('o');
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench8 = larvae::RegisterBenchmark("WaxStringView", "Contains", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            bool result = sv.Contains("lazy");
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench9 = larvae::RegisterBenchmark("WaxStringView", "StartsWith", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            bool result = sv.StartsWith("The");
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench10 = larvae::RegisterBenchmark("WaxStringView", "EndsWith", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            bool result = sv.EndsWith("dog");
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench11 = larvae::RegisterBenchmark("WaxStringView", "Compare", [](larvae::BenchmarkState& state) {
        wax::StringView sv1{"Hello World"};
        wax::StringView sv2{"Hello World"};

        while (state.KeepRunning())
        {
            int result = sv1.Compare(sv2);
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench12 = larvae::RegisterBenchmark("WaxStringView", "Equals", [](larvae::BenchmarkState& state) {
        wax::StringView sv1{"Hello World"};
        wax::StringView sv2{"Hello World"};

        while (state.KeepRunning())
        {
            bool result = sv1.Equals(sv2);
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench13 = larvae::RegisterBenchmark("WaxStringView", "EqualityOperator", [](larvae::BenchmarkState& state) {
        wax::StringView sv1{"Hello World"};
        wax::StringView sv2{"Hello World"};

        while (state.KeepRunning())
        {
            bool result = sv1 == sv2;
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench14 = larvae::RegisterBenchmark("WaxStringView", "Substr", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            wax::StringView sub = sv.Substr(10, 5);
            larvae::DoNotOptimize(sub.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench15 = larvae::RegisterBenchmark("WaxStringView", "RemovePrefix", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            wax::StringView result = sv.RemovePrefix(4);
            larvae::DoNotOptimize(result.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench16 = larvae::RegisterBenchmark("WaxStringView", "RemoveSuffix", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            wax::StringView result = sv.RemoveSuffix(4);
            larvae::DoNotOptimize(result.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    // =============================================================================
    // std::string_view Comparison Benchmarks
    // =============================================================================

    auto bench17 = larvae::RegisterBenchmark("StdStringView", "ConstructFromLiteral", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string_view sv{"Hello World"};
            larvae::DoNotOptimize(sv.data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench18 = larvae::RegisterBenchmark("StdStringView", "FindChar", [](larvae::BenchmarkState& state) {
        std::string_view sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.find('z');
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench19 = larvae::RegisterBenchmark("StdStringView", "FindSubstring", [](larvae::BenchmarkState& state) {
        std::string_view sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.find("lazy");
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench20 = larvae::RegisterBenchmark("StdStringView", "StartsWith", [](larvae::BenchmarkState& state) {
        std::string_view sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            bool result = sv.starts_with("The");
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench21 = larvae::RegisterBenchmark("StdStringView", "EndsWith", [](larvae::BenchmarkState& state) {
        std::string_view sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            bool result = sv.ends_with("dog");
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench22 = larvae::RegisterBenchmark("StdStringView", "Compare", [](larvae::BenchmarkState& state) {
        std::string_view sv1{"Hello World"};
        std::string_view sv2{"Hello World"};

        while (state.KeepRunning())
        {
            int result = sv1.compare(sv2);
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench23 = larvae::RegisterBenchmark("StdStringView", "EqualityOperator", [](larvae::BenchmarkState& state) {
        std::string_view sv1{"Hello World"};
        std::string_view sv2{"Hello World"};

        while (state.KeepRunning())
        {
            bool result = sv1 == sv2;
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench24 = larvae::RegisterBenchmark("StdStringView", "Substr", [](larvae::BenchmarkState& state) {
        std::string_view sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            std::string_view sub = sv.substr(10, 5);
            larvae::DoNotOptimize(sub.data());
        }

        state.SetItemsProcessed(state.iterations());
    });
}
