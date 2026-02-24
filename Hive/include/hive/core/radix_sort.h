#pragma once

#include <cstdint>
#include <cstring>

namespace hive
{

template<typename T>
struct SortItem
{
    uint64_t key;
    T        value;
};

namespace detail
{
    template<typename T>
    void InsertionSort(SortItem<T>* data, uint32_t count)
    {
        for (uint32_t i = 1; i < count; ++i)
        {
            auto tmp = data[i];
            uint32_t j = i;
            while (j > 0 && data[j - 1].key > tmp.key)
            {
                data[j] = data[j - 1];
                --j;
            }
            data[j] = tmp;
        }
    }
} // namespace detail

// LSB radix sort on 64-bit keys.
// Sorts `data` in-place, using `scratch` as temporary storage.
// Both arrays must have at least `count` elements.
// Falls back to insertion sort for count <= 64 (scratch unused).
template<typename T>
void RadixSort(SortItem<T>* data, SortItem<T>* scratch, uint32_t count)
{
    if (count <= 1) return;

    if (count <= 64)
    {
        detail::InsertionSort(data, count);
        return;
    }

    auto* src = data;
    auto* dst = scratch;

    for (uint32_t byte_idx = 0; byte_idx < 8; ++byte_idx)
    {
        uint32_t shift = byte_idx * 8;

        // Histogram
        uint32_t histogram[256]{};
        for (uint32_t i = 0; i < count; ++i)
        {
            uint8_t radix = static_cast<uint8_t>(src[i].key >> shift);
            histogram[radix]++;
        }

        // Skip pass if all items land in same bucket
        bool skip = false;
        for (uint32_t b = 0; b < 256; ++b)
        {
            if (histogram[b] == count) { skip = true; break; }
        }
        if (skip) continue;

        // Prefix sum
        uint32_t offsets[256];
        offsets[0] = 0;
        for (uint32_t b = 1; b < 256; ++b)
            offsets[b] = offsets[b - 1] + histogram[b - 1];

        // Scatter
        for (uint32_t i = 0; i < count; ++i)
        {
            uint8_t radix = static_cast<uint8_t>(src[i].key >> shift);
            dst[offsets[radix]++] = src[i];
        }

        // Swap buffers
        auto* tmp = src;
        src = dst;
        dst = tmp;
    }

    // If result ended up in scratch, copy back
    if (src != data)
        std::memcpy(data, src, count * sizeof(SortItem<T>));
}

} // namespace hive
