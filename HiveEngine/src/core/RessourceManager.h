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
        u32 pushData(T data);
        void putData(i32 id, T data);
        void clearData(i32 id);

        i32 size();

    private:
        std::vector<T> data_;
        std::stack<i32> available_data_slot_;
    };

    template<typename T>
    T & RessourceManager<T>::getData(u32 id)
    {
        return data_[id];
    }

    template<typename T>
    u32 RessourceManager<T>::pushData(T data)
    {
        if(available_data_slot_.empty())
        {
            data_.push_back(data);
            return data_.size() - 1;
        }
        else
        {
            i32 id = available_data_slot_.top();
            data_[id] = data;
            available_data_slot_.pop();
            return id;
        }
    }

    template<typename T>
    void RessourceManager<T>::putData(i32 id, T data)
    {
        data_[id] = data;
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
