#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace drone
{

    class IService
    {
    public:
        virtual ~IService() = default;
        virtual void Tick() = 0;
    };

    struct ServiceThreadConfig
    {
        uint32_t m_tickIntervalMs{100};
    };

    class ServiceThread
    {
    public:
        static constexpr size_t kMaxServices = 16;

        explicit ServiceThread(const ServiceThreadConfig& config = {});
        ~ServiceThread();

        ServiceThread(const ServiceThread&) = delete;
        ServiceThread& operator=(const ServiceThread&) = delete;

        void Start();
        void Stop();
        [[nodiscard]] bool IsRunning() const noexcept;

        bool Register(IService* service);
        bool Unregister(IService* service);

        void Wake();

    private:
        void ThreadMain();

        IService* m_services[kMaxServices]{};
        size_t m_serviceCount{0};
        std::mutex m_serviceMutex;

        std::mutex m_wakeMutex;
        std::condition_variable m_wakeCv;

        std::thread m_thread;
        std::atomic<bool> m_running{false};
        std::atomic<bool> m_shouldStop{false};
        uint32_t m_tickIntervalMs;
    };

} // namespace drone
