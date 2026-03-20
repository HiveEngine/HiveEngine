#include <larvae/larvae.h>

#include <drone/frame_pipeline.h>
#include <drone/job_system.h>

#include <comb/buddy_allocator.h>

#include <atomic>

namespace
{
    using TestAlloc = comb::BuddyAllocator;

    struct TestJobSystem
    {
        TestAlloc m_alloc{2 * 1024 * 1024};
        drone::JobSystem<TestAlloc> m_system{m_alloc, {2, 1024, 1024, 64 * 1024}};
        drone::JobSubmitter m_submitter{drone::MakeJobSubmitter(m_system)};

        TestJobSystem()
        {
            m_system.Start();
        }
        ~TestJobSystem()
        {
            m_system.Stop();
        }
    };

    // FrameIndex

    auto tFI1 = larvae::RegisterTest("DroneFrameIndex", "InitialState", []() {
        drone::FrameIndex idx;
        larvae::AssertEqual(idx.Write(), uint32_t{0});
        larvae::AssertEqual(idx.Read(), uint32_t{1});
    });

    auto tFI2 = larvae::RegisterTest("DroneFrameIndex", "FlipSwapsBuffers", []() {
        drone::FrameIndex idx;
        idx.Flip();
        larvae::AssertEqual(idx.Write(), uint32_t{1});
        larvae::AssertEqual(idx.Read(), uint32_t{0});
    });

    auto tFI3 = larvae::RegisterTest("DroneFrameIndex", "DoubleFlipRestores", []() {
        drone::FrameIndex idx;
        idx.Flip();
        idx.Flip();
        larvae::AssertEqual(idx.Write(), uint32_t{0});
        larvae::AssertEqual(idx.Read(), uint32_t{1});
    });

    // FrameData

    auto tFD1 = larvae::RegisterTest("DroneFrameData", "WriteAndReadAccessCorrectBuffers", []() {
        drone::FrameIndex idx;
        drone::FrameData<int> data;

        data.ForWrite(idx) = 100;
        // Read() is the opposite buffer, so it should have default value
        larvae::AssertEqual(data.ForRead(idx), 0);
        larvae::AssertEqual(data.ForWrite(idx), 100);

        // After flip, what was Write becomes Read
        idx.Flip();
        larvae::AssertEqual(data.ForRead(idx), 100);
        data.ForWrite(idx) = 200;
        larvae::AssertEqual(data.ForWrite(idx), 200);
    });

    auto tFD2 = larvae::RegisterTest("DroneFrameData", "IndexOperator", []() {
        drone::FrameData<int> data;
        data[0] = 10;
        data[1] = 20;
        larvae::AssertEqual(data[0], 10);
        larvae::AssertEqual(data[1], 20);
    });

    auto tFD3 = larvae::RegisterTest("DroneFrameData", "DoubleBufferIsolation", []() {
        drone::FrameIndex idx;
        drone::FrameData<int> data;

        // Write to buffer 0
        data.ForWrite(idx) = 42;
        idx.Flip();
        // Now buffer 1 is write, buffer 0 is read
        data.ForWrite(idx) = 99;

        // They should be independent
        larvae::AssertEqual(data.ForRead(idx), 42);
        larvae::AssertEqual(data.ForWrite(idx), 99);
    });

    // FramePipeline

    auto tFP1 = larvae::RegisterTest("DroneFramePipeline", "BeginFrameFlipsIndex", []() {
        TestJobSystem js;
        drone::FramePipeline pipeline{js.m_submitter};

        uint32_t writeBefore = pipeline.CurrentFrame().Write();
        pipeline.BeginFrame();
        uint32_t writeAfter = pipeline.CurrentFrame().Write();

        larvae::AssertNotEqual(writeBefore, writeAfter);
    });

    auto tFP2 = larvae::RegisterTest("DroneFramePipeline", "SubmitAndWaitRender", []() {
        TestJobSystem js;
        drone::FramePipeline pipeline{js.m_submitter};

        std::atomic<int> renderResult{0};

        pipeline.BeginFrame();
        pipeline.SubmitRender(
            [](void* data) {
                static_cast<std::atomic<int>*>(data)->store(1);
            },
            &renderResult);
        pipeline.WaitRender();

        larvae::AssertEqual(renderResult.load(), 1);
    });

    auto tFP3 = larvae::RegisterTest("DroneFramePipeline", "MultipleFrameCycles", []() {
        TestJobSystem js;
        drone::FramePipeline pipeline{js.m_submitter};

        std::atomic<int> frameCount{0};

        constexpr int kFrames = 5;
        for (int i = 0; i < kFrames; ++i)
        {
            pipeline.BeginFrame();
            pipeline.SubmitRender(
                [](void* data) {
                    static_cast<std::atomic<int>*>(data)->fetch_add(1);
                },
                &frameCount);
        }
        pipeline.WaitRender();

        larvae::AssertEqual(frameCount.load(), kFrames);
    });

} // namespace
