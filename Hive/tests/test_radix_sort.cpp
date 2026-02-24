#include <larvae/larvae.h>
#include <hive/core/radix_sort.h>
#include <vector>

namespace {

    using hive::SortItem;
    using hive::RadixSort;

    // =========================================================================
    // Edge cases
    // =========================================================================

    auto t_empty = larvae::RegisterTest("RadixSort", "Empty", []() {
        SortItem<int> data[1]{};
        SortItem<int> scratch[1]{};
        RadixSort(data, scratch, 0);
        larvae::AssertTrue(true);
    });

    auto t_single = larvae::RegisterTest("RadixSort", "Single", []() {
        SortItem<int> data[1]{{42, 1}};
        SortItem<int> scratch[1]{};
        RadixSort(data, scratch, 1);
        larvae::AssertEqual(data[0].key, uint64_t{42});
        larvae::AssertEqual(data[0].value, 1);
    });

    // =========================================================================
    // Small arrays (insertion sort path)
    // =========================================================================

    auto t_small_sorted = larvae::RegisterTest("RadixSort", "SmallAlreadySorted", []() {
        SortItem<int> data[5]{{1,10}, {2,20}, {3,30}, {4,40}, {5,50}};
        SortItem<int> scratch[5]{};
        RadixSort(data, scratch, 5);
        for (uint32_t i = 0; i < 5; ++i)
            larvae::AssertEqual(data[i].key, static_cast<uint64_t>(i + 1));
    });

    auto t_small_reverse = larvae::RegisterTest("RadixSort", "SmallReverse", []() {
        SortItem<int> data[5]{{5,50}, {4,40}, {3,30}, {2,20}, {1,10}};
        SortItem<int> scratch[5]{};
        RadixSort(data, scratch, 5);
        for (uint32_t i = 0; i < 5; ++i)
        {
            larvae::AssertEqual(data[i].key, static_cast<uint64_t>(i + 1));
            larvae::AssertEqual(data[i].value, static_cast<int>((i + 1) * 10));
        }
    });

    auto t_small_stability = larvae::RegisterTest("RadixSort", "SmallStability", []() {
        SortItem<int> data[4]{{7,0}, {7,1}, {7,2}, {7,3}};
        SortItem<int> scratch[4]{};
        RadixSort(data, scratch, 4);
        for (uint32_t i = 0; i < 4; ++i)
            larvae::AssertEqual(data[i].value, static_cast<int>(i));
    });

    // =========================================================================
    // Large arrays (radix sort path)
    // =========================================================================

    auto t_large_reverse = larvae::RegisterTest("RadixSort", "LargeReverse", []() {
        constexpr uint32_t N = 200;
        std::vector<SortItem<uint32_t>> data(N);
        std::vector<SortItem<uint32_t>> scratch(N);
        for (uint32_t i = 0; i < N; ++i)
            data[i] = {N - i, i};

        RadixSort(data.data(), scratch.data(), N);
        for (uint32_t i = 0; i < N; ++i)
            larvae::AssertEqual(data[i].key, static_cast<uint64_t>(i + 1));
    });

    auto t_large_random = larvae::RegisterTest("RadixSort", "LargeRandom", []() {
        constexpr uint32_t N = 500;
        std::vector<SortItem<uint32_t>> data(N);
        std::vector<SortItem<uint32_t>> scratch(N);

        uint32_t state = 12345;
        for (uint32_t i = 0; i < N; ++i)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            data[i] = {state, i};
        }

        RadixSort(data.data(), scratch.data(), N);
        for (uint32_t i = 1; i < N; ++i)
            larvae::AssertTrue(data[i - 1].key <= data[i].key);
    });

    auto t_large_stability = larvae::RegisterTest("RadixSort", "LargeStability", []() {
        constexpr uint32_t N = 100;
        std::vector<SortItem<uint32_t>> data(N);
        std::vector<SortItem<uint32_t>> scratch(N);
        for (uint32_t i = 0; i < N; ++i)
            data[i] = {i / 10, i};

        RadixSort(data.data(), scratch.data(), N);
        for (uint32_t g = 0; g < 10; ++g)
            for (uint32_t j = 1; j < 10; ++j)
            {
                uint32_t idx = g * 10 + j;
                larvae::AssertTrue(data[idx].value > data[idx - 1].value);
            }
    });

    auto t_large_high_bits = larvae::RegisterTest("RadixSort", "LargeHighBits", []() {
        constexpr uint32_t N = 100;
        std::vector<SortItem<uint32_t>> data(N);
        std::vector<SortItem<uint32_t>> scratch(N);
        for (uint32_t i = 0; i < N; ++i)
            data[i] = {static_cast<uint64_t>(N - i) << 48, i};

        RadixSort(data.data(), scratch.data(), N);
        for (uint32_t i = 1; i < N; ++i)
            larvae::AssertTrue(data[i - 1].key <= data[i].key);
    });

    auto t_threshold_boundary = larvae::RegisterTest("RadixSort", "ThresholdBoundary", []() {
        constexpr uint32_t N = 64;
        std::vector<SortItem<uint32_t>> data(N);
        std::vector<SortItem<uint32_t>> scratch(N);
        for (uint32_t i = 0; i < N; ++i)
            data[i] = {N - i, i};

        RadixSort(data.data(), scratch.data(), N);
        for (uint32_t i = 0; i < N; ++i)
            larvae::AssertEqual(data[i].key, static_cast<uint64_t>(i + 1));
    });

    auto t_above_threshold = larvae::RegisterTest("RadixSort", "AboveThreshold", []() {
        constexpr uint32_t N = 65;
        std::vector<SortItem<uint32_t>> data(N);
        std::vector<SortItem<uint32_t>> scratch(N);
        for (uint32_t i = 0; i < N; ++i)
            data[i] = {N - i, i};

        RadixSort(data.data(), scratch.data(), N);
        for (uint32_t i = 0; i < N; ++i)
            larvae::AssertEqual(data[i].key, static_cast<uint64_t>(i + 1));
    });

} // namespace
