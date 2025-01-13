#include "Memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "Logger.h"

struct hive::Memory::Stats
{
    u64 total_allocated = 0;
    u64 tagged_allocated[Tag::COUNT]{};
    u64 allocation_count = 0;
};

hive::Memory::Stats* hive::Memory::s_stats_ = nullptr;

hive::Memory::Memory()
{
    s_stats_ = new Stats();
}

hive::Memory::~Memory()
{
    assert(s_stats_->total_allocated == 0); //Check for memory leak
    free(s_stats_);
}

void * hive::Memory::allocate(u64 size, Tag tag)
{
    s_stats_->total_allocated += size;
    s_stats_->tagged_allocated[tag] += size;
    s_stats_->allocation_count++;

    return malloc(size);
}

void hive::Memory::release(void *block, u64 size, Tag tag)
{
    s_stats_->total_allocated -= size;
    s_stats_->tagged_allocated[tag] -= size;

    free(block);
}

void hive::Memory::copy(const void *src, void *dest, const u64 size)
{
    std::memcpy(dest, src, size);
}



static const char* memory_tag_strings[hive::Memory::Tag::COUNT] = {
    "UNKNOWN    ",
    "Engine     ",
    "Renderer   ",
    "Game       ",
    "STRING     ",
    };

u64 string_length(const char* str) {
    return strlen(str);
}

char* string_duplicate(const char* str) {
    u64 length = string_length(str);
    char* copy = static_cast<char *>(hive::Memory::allocate(length + 1, hive::Memory::Tag::STRING));
    hive::Memory::copy(str, copy, length + 1);
    return copy;
}

void hive::Memory::printMemoryUsage()
{
    const u64 gib = 1024 * 1024 * 1024;
    const u64 mib = 1024 * 1024;
    const u64 kib = 1024;

    char buffer[8000] = "System memory use (tagged):\n";
    u64 offset = strlen(buffer);
    for (u32 i = 0; i < Tag::COUNT; ++i) {
        char unit[4] = "XiB";
        float amount = 1.0f;
        if (s_stats_->tagged_allocated[i] >= gib) {
            unit[0] = 'G';
            amount = s_stats_->tagged_allocated[i] / (float)gib;
        } else if (s_stats_->tagged_allocated[i] >= mib) {
            unit[0] = 'M';
            amount = s_stats_->tagged_allocated[i] / (float)mib;
        } else if (s_stats_->tagged_allocated[i] >= kib) {
            unit[0] = 'K';
            amount = s_stats_->tagged_allocated[i] / (float)kib;
        } else {
            unit[0] = 'B';
            unit[1] = 0;
            amount = (float)s_stats_->tagged_allocated[i];
        }

        i32 length = snprintf(buffer + offset, 8000, "  %s: %.2f%s\n", memory_tag_strings[i], amount, unit);
        offset += length;
    }

    LOG_INFO(buffer);
}

