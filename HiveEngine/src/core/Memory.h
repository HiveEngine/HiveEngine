#pragma once

#include <hvpch.h>
namespace hive
{
    class Memory
    {
    public:
        enum Tag
        {
            UNKNOWN,
            ENGINE,
            RENDERER,
            GAME,
            STRING,

            COUNT
        };

        explicit Memory();
        ~Memory();


        static void* allocate(u64 size, Tag tag);
        static void release(void* block, u64 size, Tag tag);

        static void copy(const void* src, void* dest, u64 size);

        static void printMemoryUsage();


    private:
        struct Stats;
        static Stats* s_stats_;
    };
}