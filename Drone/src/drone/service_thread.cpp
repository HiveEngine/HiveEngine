#include <drone/service_thread.h>

#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>

namespace drone
{
    ServiceThread::ServiceThread(const ServiceThreadConfig& config)
        : m_tickIntervalMs{config.m_tickIntervalMs}
    {
    }

    ServiceThread::~ServiceThread()
    {
        Stop();
    }

    void ServiceThread::Start()
    {
        if (m_running.exchange(true, std::memory_order_acq_rel))
            return;

        m_shouldStop.store(false, std::memory_order_release);
        m_thread = std::thread{&ServiceThread::ThreadMain, this};
    }

    void ServiceThread::Stop()
    {
        if (!m_running.exchange(false, std::memory_order_acq_rel))
            return;

        m_shouldStop.store(true, std::memory_order_release);
        m_wakeCv.notify_one();

        if (m_thread.joinable())
            m_thread.join();
    }

    bool ServiceThread::IsRunning() const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }

    bool ServiceThread::Register(IService* service)
    {
        hive::Assert(service != nullptr, "ServiceThread: null service");
        std::lock_guard<std::mutex> lock{m_serviceMutex};
        if (m_serviceCount >= kMaxServices)
            return false;
        m_services[m_serviceCount++] = service;
        return true;
    }

    bool ServiceThread::Unregister(IService* service)
    {
        std::lock_guard<std::mutex> lock{m_serviceMutex};
        for (size_t i = 0; i < m_serviceCount; ++i)
        {
            if (m_services[i] == service)
            {
                m_services[i] = m_services[m_serviceCount - 1];
                m_services[m_serviceCount - 1] = nullptr;
                --m_serviceCount;
                return true;
            }
        }
        return false;
    }

    void ServiceThread::Wake()
    {
        m_wakeCv.notify_one();
    }

    void ServiceThread::ThreadMain()
    {
        HIVE_PROFILE_THREAD("ServiceThread");

        while (!m_shouldStop.load(std::memory_order_acquire))
        {
            {
                HIVE_PROFILE_SCOPE_N("ServiceThread::TickAll");
                std::lock_guard<std::mutex> lock{m_serviceMutex};
                for (size_t i = 0; i < m_serviceCount; ++i)
                {
                    m_services[i]->Tick();
                }
            }

            std::unique_lock<std::mutex> lock{m_wakeMutex};
            m_wakeCv.wait_for(lock, std::chrono::milliseconds{m_tickIntervalMs},
                              [this] { return m_shouldStop.load(std::memory_order_relaxed); });
        }
    }

} // namespace drone
