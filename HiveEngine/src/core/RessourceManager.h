#pragma once

#include <hvpch.h>
#include <vector>
#include <stack>
namespace hive
{
    template<typename T>
    class RessourceManager
    {
    public:
        T& getData(u32 id);
        i32 getAvailableId();
        u32 pushData(T data);
        void clearData(i32 id);


        i32 size();

    private:
        std::vector<T> data_;
        std::stack<i32> available_data_slot_;
    };

    template<typename T>
    T& RessourceManager<T>::getData(u32 id)
    {
        return data_[id];
    }

    template<typename T>
    i32 RessourceManager<T>::getAvailableId()
    {
        if(available_data_slot_.empty())
        {
            return -1;
        }

        i32 id = available_data_slot_.top();
        available_data_slot_.pop();
        return id;
    }

    template<typename T>
    u32 RessourceManager<T>::pushData(T data)
    {
        data_.emplace_back(data);
        return data_.size() - 1;
    }


    template<typename T>
    void RessourceManager<T>::clearData(i32 id)
    {
        available_data_slot_.push(id);
    }

    template<typename T>
    i32 RessourceManager<T>::size()
    {
        return data_.size();
    }
}
