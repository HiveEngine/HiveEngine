#include <nectar/io/io_scheduler.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <hive/profiling/profiler.h>
#include <cstdio>

namespace nectar
{
    IOScheduler::IOScheduler(VirtualFilesystem& vfs, comb::DefaultAllocator& alloc,
                             IOSchedulerConfig config)
        : vfs_{&vfs}
        , alloc_{&alloc}
        , request_queue_{alloc}
        , completion_queue_{alloc}
        , cancelled_ids_{alloc}
    {
        workers_.reserve(config.worker_count);
        for (size_t i = 0; i < config.worker_count; ++i)
            workers_.emplace_back(&IOScheduler::WorkerLoop, this);
    }

    IOScheduler::~IOScheduler()
    {
        Shutdown();
    }

    void IOScheduler::Shutdown()
    {
        bool expected = false;
        if (!shutdown_.compare_exchange_strong(expected, true))
            return;  // already shut down

        request_cv_.notify_all();
        for (auto& w : workers_)
        {
            if (w.joinable())
                w.join();
        }
    }

    IORequestId IOScheduler::Submit(wax::StringView path, LoadPriority priority)
    {
        HIVE_PROFILE_SCOPE_N("IOScheduler::Submit");
        std::lock_guard<std::mutex> lock{request_mutex_};

        IORequestId id = next_id_++;

        IORequest req;
        req.id = id;
        req.path = wax::String<>{*alloc_};
        req.path.Append(path.Data(), path.Size());
        req.priority = priority;
        req.cancelled = false;

        request_queue_.PushBack(static_cast<IORequest&&>(req));
        request_cv_.notify_one();
        return id;
    }

    void IOScheduler::Cancel(IORequestId id)
    {
        std::lock_guard<std::mutex> lock{request_mutex_};

        // Try to find in pending queue
        for (size_t i = 0; i < request_queue_.Size(); ++i)
        {
            if (request_queue_[i].id == id)
            {
                request_queue_[i].cancelled = true;
                return;
            }
        }

        // Not in queue — may be in-flight, track for discard on completion
        cancelled_ids_.Insert(id);
    }

    size_t IOScheduler::DrainCompletions(wax::Vector<IOCompletion>& out)
    {
        HIVE_PROFILE_SCOPE_N("IOScheduler::DrainCompletions");
        // Phase 1: drain all completions
        wax::Vector<IOCompletion> raw{*alloc_};
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{completion_mutex_};
            raw = static_cast<wax::Vector<IOCompletion>&&>(completion_queue_);
            completion_queue_ = wax::Vector<IOCompletion>{*alloc_};
        }

        // Phase 2: filter cancelled (separate lock to avoid deadlock)
        {
            std::lock_guard<std::mutex> lock{request_mutex_};
            for (size_t i = 0; i < raw.Size(); ++i)
            {
                if (cancelled_ids_.Contains(raw[i].request_id))
                {
                    cancelled_ids_.Remove(raw[i].request_id);
                }
                else
                {
                    out.PushBack(static_cast<IOCompletion&&>(raw[i]));
                }
            }
        }

        return out.Size();
    }

    size_t IOScheduler::PendingCount() const
    {
        std::lock_guard<std::mutex> lock{request_mutex_};
        return request_queue_.Size();
    }

    bool IOScheduler::IsShutdown() const noexcept
    {
        return shutdown_.load(std::memory_order_relaxed);
    }

    void IOScheduler::WorkerLoop()
    {
        int id = worker_id_counter_.fetch_add(1);
        char thread_name[32];
        std::snprintf(thread_name, sizeof(thread_name), "Nectar-IO-%d", id);
        HIVE_PROFILE_THREAD(thread_name);
        HIVE_PROFILE_SCOPE_N("IOScheduler::WorkerLoop");

        while (true)
        {
            IORequest req;

            // Wait for work
            {
                std::unique_lock<std::mutex> lock{request_mutex_};
                request_cv_.wait(lock, [this] {
                    return shutdown_.load(std::memory_order_relaxed) || !request_queue_.IsEmpty();
                });

                if (shutdown_.load(std::memory_order_relaxed) && request_queue_.IsEmpty())
                    break;

                // Find highest priority (lowest enum value)
                size_t best = 0;
                for (size_t i = 1; i < request_queue_.Size(); ++i)
                {
                    if (static_cast<uint8_t>(request_queue_[i].priority) <
                        static_cast<uint8_t>(request_queue_[best].priority))
                    {
                        best = i;
                    }
                }

                // Swap-remove
                req = static_cast<IORequest&&>(request_queue_[best]);
                if (best < request_queue_.Size() - 1)
                    request_queue_[best] = static_cast<IORequest&&>(request_queue_[request_queue_.Size() - 1]);
                request_queue_.PopBack();
            }

            // Skip cancelled
            if (req.cancelled)
                continue;

            // Execute read
            auto buffer = vfs_->ReadSync(req.path.View());

            IOCompletion completion;
            completion.request_id = req.id;
            completion.success = (buffer.Size() > 0);
            completion.data = static_cast<wax::ByteBuffer<>&&>(buffer);

            {
                std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{completion_mutex_};
                completion_queue_.PushBack(static_cast<IOCompletion&&>(completion));
            }
        }
    }
}
