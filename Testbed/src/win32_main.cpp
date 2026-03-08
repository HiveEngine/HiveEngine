#include <terra/platform/glfw_terra.h>
#include <terra/terra.h>

#include <testbed/precomp.h>
#define TERRA_NATIVE_WIN32
#include <hive/core/log.h>

#include <swarm/platform/diligent_swarm.h>
#include <swarm/platform/win32_swarm.h>
#include <swarm/swarm.h>

#include <terra/terra_native.h>

#include <iostream>

struct PlatformContext
{
    swarm::RenderContext* renderContext_;
    terra::WindowContext* windowContext_;
};

static void GameLogic(PlatformContext& context) {
    const terra::InputState* currentInput = terra::GetWindowInputState(context.windowContext_);

    if (terra::IsKeyDown(currentInput, terra::Key::A))
    {
        std::cout << "A" << std::endl;
    }

    swarm::Render(context.renderContext_);
}

class Engine
{
public:
    Engine();
    ~Engine() = default;

    void Run();

private:
    bool Init();
    void Shutdown();

    void Loop();

    hive::LogManager logManager_;
    hive::ConsoleLogger consoleLogger_;

    terra::WindowContext windowContext_;
    swarm::RenderContext renderContext_;
};

Engine::Engine()
    : consoleLogger_(logManager_) {}

void Engine::Run() {
    if (!Init())
    {
        return;
    }

    Loop();

    Shutdown();
}

bool Engine::Init() {
    terra::SetWindowTitle(&windowContext_, "Hive Engine");
    terra::SetWindowSize(&windowContext_, 1280, 720);

    if (!terra::InitSystem() || !terra::InitWindowContext(&windowContext_))
    {
        return false;
    }

    if (!swarm::InitSystem())
    {
        return false;
    }

    terra::NativeWindow nativeWindow = terra::GetNativeWindow(&windowContext_);
    if (!swarm::InitRenderContextWin32(&renderContext_, nativeWindow.m_instance, nativeWindow.m_window,
                                       static_cast<uint32_t>(terra::GetWindowWidth(&windowContext_)),
                                       static_cast<uint32_t>(terra::GetWindowHeight(&windowContext_))))
    {
        return false;
    }

    swarm::SetupGraphicPipeline(renderContext_);
    return true;
}

void Engine::Shutdown() {
    swarm::ShutdownRenderContext(renderContext_);
    swarm::ShutdownSystem();

    terra::ShutdownWindowContext(&windowContext_);
    terra::ShutdownSystem();
}

void Engine::Loop() {
    PlatformContext platformContext{&renderContext_, &windowContext_};
    while (!terra::ShouldWindowClose(platformContext.windowContext_))
    {
        terra::PollEvents(platformContext.windowContext_);
        GameLogic(platformContext);
    }
}

int main() {
    Engine engine;
    engine.Run();
}
