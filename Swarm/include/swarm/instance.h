#pragma once

namespace swarm
{
    struct InstanceDescription
    {
        const char *applicationName;
        unsigned int applicationVersion;
        bool isDebug;
    };

    class Instance
    {
    public:
        Instance(InstanceDescription instanceDescription);

        ~Instance();

        bool IsValid() const;

        // Delete copy constructor and copy assignment operator
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

    private:
        #include <swarm/instancebackend.inl>
    };
}
