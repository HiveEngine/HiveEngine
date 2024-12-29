#pragma once

#include <hvpch.h>
#include <utility>

#include <new>

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

        template<typename T, Tag tag, typename ...Args>
        static T* createObject(Args&& ... args);

        template<typename  T, Tag tag>
        static void destroyObject(void *ptr);

        static void* allocate(u64 size, Tag tag);
        static void release(void* block, u64 size, Tag tag);

        static void copy(const void* src, void* dest, u64 size);

        static void printMemoryUsage();


    private:
        struct Stats;
        static Stats* s_stats_;
    };


    //TODO: research if the static_cast has some performance cost for that
    template<typename T, Memory::Tag tag, typename ... Args>
    T *Memory::createObject(Args &&... args)
    {
        T* ptr = static_cast<T*>(allocate(sizeof(T), tag)); //Allocate the memory
        new (ptr) T(std::forward<Args>(args)...); //Call the constructor of the object on the allocated memory
        return ptr;
    }

    //TODO: research if the static_cast has some performance cost for that
    template<typename T, Memory::Tag tag>
    void Memory::destroyObject(void *ptr)
    {
        T* t_ptr = static_cast<T*>(ptr);
        t_ptr->~T();

        release(ptr, sizeof(T), tag);
    }
}
