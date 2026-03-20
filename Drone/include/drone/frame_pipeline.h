#pragma once

#include <drone/counter.h>
#include <drone/job_submitter.h>
#include <drone/job_types.h>

#include <cstdint>

namespace drone
{
    // Tracks which of two buffers is "write" (game thread) vs "read" (render).
    struct FrameIndex
    {
        [[nodiscard]] uint32_t Write() const noexcept
        {
            return m_value;
        }

        [[nodiscard]] uint32_t Read() const noexcept
        {
            return m_value ^ 1;
        }

        void Flip() noexcept
        {
            m_value ^= 1;
        }

    private:
        uint32_t m_value{0};
    };

    // Double-buffered data slot. Game writes [Write], render reads [Read].
    template <typename T> class FrameData
    {
    public:
        T& ForWrite(const FrameIndex& idx) noexcept
        {
            return m_data[idx.Write()];
        }

        const T& ForRead(const FrameIndex& idx) const noexcept
        {
            return m_data[idx.Read()];
        }

        T& operator[](uint32_t i) noexcept
        {
            return m_data[i];
        }

        const T& operator[](uint32_t i) const noexcept
        {
            return m_data[i];
        }

    private:
        T m_data[2]{};
    };

    // Manages the frame overlap between simulation and rendering.
    // BeginFrame waits for the previous render job to finish, then flips buffers.
    // EndFrame submits a render job and returns immediately.
    class FramePipeline
    {
    public:
        explicit FramePipeline(JobSubmitter jobs)
            : m_jobs{jobs}
        {
        }

        // Wait for previous render job to complete, then flip frame buffers.
        void BeginFrame()
        {
            m_renderDone.Wait();
            m_frameIndex.Flip();
        }

        // Submit a render job. The job reads FrameData[Read()] and calls
        // Vulkan/Swarm commands. Returns immediately — render runs on a worker.
        // `func` signature: void(void* userData)
        void SubmitRender(JobDecl::Func func, void* userData)
        {
            m_renderDone.Reset(0);

            JobDecl job;
            job.m_func = func;
            job.m_userData = userData;
            job.m_priority = Priority::HIGH;

            m_jobs.Submit(job, m_renderDone);
        }

        // Block until the current render job completes (for shutdown).
        void WaitRender()
        {
            m_renderDone.Wait();
        }

        [[nodiscard]] const FrameIndex& CurrentFrame() const noexcept
        {
            return m_frameIndex;
        }

    private:
        JobSubmitter m_jobs;
        FrameIndex m_frameIndex;
        Counter m_renderDone;
    };
} // namespace drone
