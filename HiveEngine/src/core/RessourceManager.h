#pragma once

#include <hvpch.h>
#include <vector>
#include <stack>
namespace hive
{
    template<typename T>
    struct IsValidHandle
    {
        static constexpr bool value = std::is_same_v<decltype(T::id), u32>;
    };

    template<typename Data_T, typename Handle_T>
    class RessourceManager
    {
        static_assert(IsValidHandle<Handle_T>::value, "Handle_T must be a proper handle with a property id of type u32");
    public:
        explicit RessourceManager(u32 size);

        Data_T& getData(Handle_T id);
        Handle_T getAvailableId();
        u32 pushData(Data_T data);
        void clearData(Handle_T id);


        i32 size();

    private:
        std::vector<Data_T> data_;
        std::stack<Handle_T> available_data_slot_;
    };

    template<typename Data_T, typename Handle_T>
    RessourceManager<Data_T, Handle_T>::RessourceManager(u32 size) : data_(size)
    {
        for(u32 i = 0; i < size; i++)
        {
            available_data_slot_.push({i});
        }
    }

    template<typename Data_T, typename Handle_T>
    Data_T & RessourceManager<Data_T, Handle_T>::getData(Handle_T id)
    {
        return data_[id.id];
    }

    template<typename Data_T, typename Handle_T>
    Handle_T RessourceManager<Data_T, Handle_T>::getAvailableId()
    {
        if(!available_data_slot_.empty())
        {
            auto t = available_data_slot_.top();
            available_data_slot_.pop();
            return t;
        }
        return {U32_MAX};
    }

    template<typename Data_T, typename Handle_T>
    u32 RessourceManager<Data_T, Handle_T>::pushData(Data_T data)
    {
        return 0;
    }

    template<typename Data_T, typename Handle_T>
    void RessourceManager<Data_T, Handle_T>::clearData(Handle_T id)
    {

    }

    template<typename Data_T, typename Handle_T>
    i32 RessourceManager<Data_T, Handle_T>::size()
    {
        return 0;
    }
}
