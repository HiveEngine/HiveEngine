#include <larvae/larvae.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/linear_allocator.h>
#include <string>

namespace
{
    // =============================================================================
    // Wax::String SSO Benchmarks
    // =============================================================================

    auto bench1 = larvae::RegisterBenchmark("WaxString", "ConstructSmallString", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::String<comb::LinearAllocator> str{alloc, "Hello"};
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench2 = larvae::RegisterBenchmark("WaxString", "ConstructLargeString", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::String<comb::LinearAllocator> str{alloc, "This is a very long string that exceeds SSO capacity and requires heap allocation"};
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench3 = larvae::RegisterBenchmark("WaxString", "AppendSmallStrings", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::String<comb::LinearAllocator> str{alloc};
            for (int i = 0; i < 10; ++i)
            {
                str.Append("Hi");
            }
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations() * 10);
    });

    auto bench4 = larvae::RegisterBenchmark("WaxString", "AppendLargeStrings", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::String<comb::LinearAllocator> str{alloc};
            for (int i = 0; i < 10; ++i)
            {
                str.Append("This is a longer string for testing");
            }
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations() * 10);
    });

    auto bench5 = larvae::RegisterBenchmark("WaxString", "AppendChars", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::String<comb::LinearAllocator> str{alloc};
            for (int i = 0; i < 100; ++i)
            {
                str.Append('a');
            }
            larvae::DoNotOptimize(str.Data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench6 = larvae::RegisterBenchmark("WaxString", "FindChar", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = str.Find('z');
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench7 = larvae::RegisterBenchmark("WaxString", "FindSubstring", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str{alloc, "The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = str.Find("lazy");
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench8 = larvae::RegisterBenchmark("WaxString", "Compare", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024};
        wax::String<comb::LinearAllocator> str1{alloc, "Hello World"};
        wax::String<comb::LinearAllocator> str2{alloc, "Hello World"};

        while (state.KeepRunning())
        {
            bool result = str1 == str2;
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench9 = larvae::RegisterBenchmark("WaxString", "CopySmallString", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};
        wax::String<comb::LinearAllocator> source{alloc, "Hello"};

        while (state.KeepRunning())
        {
            wax::String<comb::LinearAllocator> copy{source};
            larvae::DoNotOptimize(copy.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench10 = larvae::RegisterBenchmark("WaxString", "CopyLargeString", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::String<comb::LinearAllocator> source{alloc, "This is a very long string that exceeds SSO capacity"};
            wax::String<comb::LinearAllocator> copy{source};
            larvae::DoNotOptimize(copy.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    // =============================================================================
    // std::string Comparison Benchmarks
    // =============================================================================

    auto bench11 = larvae::RegisterBenchmark("StdString", "ConstructSmallString", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string str{"Hello"};
            larvae::DoNotOptimize(str.data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench12 = larvae::RegisterBenchmark("StdString", "ConstructLargeString", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string str{"This is a very long string that exceeds SSO capacity and requires heap allocation"};
            larvae::DoNotOptimize(str.data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench13 = larvae::RegisterBenchmark("StdString", "AppendSmallStrings", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string str;
            for (int i = 0; i < 10; ++i)
            {
                str.append("Hi");
            }
            larvae::DoNotOptimize(str.data());
        }

        state.SetItemsProcessed(state.iterations() * 10);
    });

    auto bench14 = larvae::RegisterBenchmark("StdString", "AppendLargeStrings", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string str;
            for (int i = 0; i < 10; ++i)
            {
                str.append("This is a longer string for testing");
            }
            larvae::DoNotOptimize(str.data());
        }

        state.SetItemsProcessed(state.iterations() * 10);
    });

    auto bench15 = larvae::RegisterBenchmark("StdString", "AppendChars", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::string str;
            for (int i = 0; i < 100; ++i)
            {
                str.push_back('a');
            }
            larvae::DoNotOptimize(str.data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench16 = larvae::RegisterBenchmark("StdString", "FindChar", [](larvae::BenchmarkState& state) {
        std::string str{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = str.find('z');
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench17 = larvae::RegisterBenchmark("StdString", "FindSubstring", [](larvae::BenchmarkState& state) {
        std::string str{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = str.find("lazy");
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench18 = larvae::RegisterBenchmark("StdString", "Compare", [](larvae::BenchmarkState& state) {
        std::string str1{"Hello World"};
        std::string str2{"Hello World"};

        while (state.KeepRunning())
        {
            bool result = str1 == str2;
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench19 = larvae::RegisterBenchmark("StdString", "CopySmallString", [](larvae::BenchmarkState& state) {
        std::string source{"Hello"};

        while (state.KeepRunning())
        {
            std::string copy{source};
            larvae::DoNotOptimize(copy.data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench20 = larvae::RegisterBenchmark("StdString", "CopyLargeString", [](larvae::BenchmarkState& state) {
        std::string source{"This is a very long string that exceeds SSO capacity"};

        while (state.KeepRunning())
        {
            std::string copy{source};
            larvae::DoNotOptimize(copy.data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    // =============================================================================
    // StringView Benchmarks
    // =============================================================================

    auto bench21 = larvae::RegisterBenchmark("WaxStringView", "ConstructFromLiteral", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            wax::StringView sv{"Hello World"};
            larvae::DoNotOptimize(sv.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench22 = larvae::RegisterBenchmark("WaxStringView", "FindChar", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.Find('z');
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench23 = larvae::RegisterBenchmark("WaxStringView", "FindSubstring", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            size_t pos = sv.Find("lazy");
            larvae::DoNotOptimize(pos);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench24 = larvae::RegisterBenchmark("WaxStringView", "Compare", [](larvae::BenchmarkState& state) {
        wax::StringView sv1{"Hello World"};
        wax::StringView sv2{"Hello World"};

        while (state.KeepRunning())
        {
            bool result = sv1 == sv2;
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench25 = larvae::RegisterBenchmark("WaxStringView", "Substr", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            wax::StringView sub = sv.Substr(10, 5);
            larvae::DoNotOptimize(sub.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench26 = larvae::RegisterBenchmark("WaxStringView", "StartsWith", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            bool result = sv.StartsWith("The");
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench27 = larvae::RegisterBenchmark("WaxStringView", "EndsWith", [](larvae::BenchmarkState& state) {
        wax::StringView sv{"The quick brown fox jumps over the lazy dog"};

        while (state.KeepRunning())
        {
            bool result = sv.EndsWith("dog");
            larvae::DoNotOptimize(result);
        }

        state.SetItemsProcessed(state.iterations());
    });
}
