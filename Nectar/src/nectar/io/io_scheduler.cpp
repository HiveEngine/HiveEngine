#include <nectar/io/io_scheduler.h>
#include <nectar/vfs/virtual_filesystem.h>

#include <cstdio>

namespace nectar
{
    IOScheduler::IOScheduler(VirtualFilesystem& vfs, comb::DefaultAllocator& alloc, IOSchedulerConfig config)
        : m_vfs{&vfs}
        , m_alloc{&alloc}
        , m_requestQueue{alloc}
        , m_completionQueue{alloc}
        , m_cancelledIds{alloc} {
        m_workers.reserve(config.m_workerCount);
        for (size_t i = 0; i < config.m_workerCount; ++i)
        {
            m_workers.emplace_back(&IOScheduler::WorkerLoop, this);
        }
    }

    IOScheduler::~IOScheduler() {
        Shutdown();
    }

    void IOScheduler::Shutdown() {
        bool expected = false;
        if (!m_shutdown.compare_exchange_strong(expected, true))
        {
            return;
        }

        m_requestCv.notify_all();
        for (auto& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    IORequestId IOScheduler::Submit(wax::StringView path, LoadPriority priority) {
        HIVE_PROFILE_SCOPE_N("IOScheduler::Submit");
        std::lock_guard<std::mutex> lock{m_requestMutex};

        IORequestId id = m_nextId++;

        IORequest req;
        req.m_id = id;
        req.m_path = wax::String{*m_alloc};
        req.m_path.Append(path.Data(), path.Size());
        req.m_priority = priority;
        req.m_cancelled = false;

        m_requestQueue.PushBack(static_cast<IORequest&&>(req));
        m_requestCv.notify_one();
        return id;
    }

    void IOScheduler::Cancel(IORequestId id) {
        std::lock_guard<std::mutex> lock{m_requestMutex};

        for (size_t i = 0; i < m_requestQueue.Size(); ++i)
        {
            if (m_requestQueue[i].m_id == id)
            {
                m_requestQueue[i].m_cancelled = true;
                return;
            }
        }

        m_cancelledIds.Insert(id);
    }

    size_t IOScheduler::DrainCompletions(wax::Vector<IOCompletion>& out) {
        HIVE_PROFILE_SCOPE_N("IOScheduler::DrainCompletions");

        wax::Vector<IOCompletion> raw{*m_alloc};
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_completionMutex};
            raw = static_cast<wax::Vector<IOCompletion>&&>(m_completionQueue);
            m_completionQueue = wax::Vector<IOCompletion>{*m_alloc};
        }

        {
            std::lock_guard<std::mutex> lock{m_requestMutex};
            for (size_t i = 0; i < raw.Size(); ++i)
            {
                if (m_cancelledIds.Contains(raw[i].m_requestId))
                {
                    m_cancelledIds.Remove(raw[i].m_requestId);
                }
                else
                {
                    out.PushBack(static_cast<IOCompletion&&>(raw[i]));
                }
            }
        }

        return out.Size();
    }

    size_t IOScheduler::PendingCount() const {
        std::lock_guard<std::mutex> lock{m_requestMutex};
        return m_requestQueue.Size();
    }

    bool IOScheduler::IsShutdown() const noexcept {
        return m_shutdown.load(std::memory_order_relaxed);
    }

    void IOScheduler::WorkerLoop() {
        int id = m_workerIdCounter.fetch_add(1);
        char threadName[32];
        std::snprintf(threadName, sizeof(threadName), "Nectar-IO-%d", id);
        HIVE_PROFILE_THREAD(threadName);
        HIVE_PROFILE_SCOPE_N("IOScheduler::WorkerLoop");

        while (true)
        {
            IORequest req;

            {
                std::unique_lock<std::mutex> lock{m_requestMutex};
                m_requestCv.wait(
                    lock, [this] { return m_shutdown.load(std::memory_order_relaxed) || !m_requestQueue.IsEmpty(); });

                if (m_shutdown.load(std::memory_order_relaxed) && m_requestQueue.IsEmpty())
                {
                    break;
                }

                size_t best = 0;
                for (size_t i = 1; i < m_requestQueue.Size(); ++i)
                {
                    if (static_cast<uint8_t>(m_requestQueue[i].m_priority) <
                        static_cast<uint8_t>(m_requestQueue[best].m_priority))
                    {
                        best = i;
                    }
                }

                req = static_cast<IORequest&&>(m_requestQueue[best]);
                if (best < m_requestQueue.Size() - 1)
                {
                    m_requestQueue[best] = static_cast<IORequest&&>(m_requestQueue[m_requestQueue.Size() - 1]);
                }
                m_requestQueue.PopBack();
            }

            if (req.m_cancelled)
            {
                continue;
            }

            auto buffer = m_vfs->ReadSync(req.m_path.View());

            IOCompletion completion;
            completion.m_requestId = req.m_id;
            completion.m_success = (buffer.Size() > 0);
            completion.m_data = static_cast<wax::ByteBuffer&&>(buffer);

            {
                std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_completionMutex};
                m_completionQueue.PushBack(static_cast<IOCompletion&&>(completion));
            }
        }
    }
} // namespace nectar
