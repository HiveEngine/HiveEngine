#pragma once


namespace swarm
{
    struct SurfaceDescription
    {
        void* windowHandle{nullptr};
        void* displayHandle{nullptr};

        unsigned long windowId{0};

        enum class SessionType
        {
            WIN32, X11, WAYLAND
        };
        SessionType type{SessionType::WIN32};
    };

    class Instance;
    class Surface
    {
    public:
        Surface(Instance& instance, SurfaceDescription const& description);
        ~Surface();

        Surface(const Surface&) = delete;
        Surface& operator=(const Surface&) = delete;

    protected:
        Instance& m_Instance;

    private:
        #include <swarm/surfacebackend.inl>
    };
}
