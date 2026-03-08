#include <terra/platform/glfw_terra.h>
#include <terra/terra.h>

#include <antennae/actions.h>
#include <antennae/input.h>

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
    antennae::Keyboard* keyboard_;
    antennae::Mouse* mouse_;
    antennae::InputActions* actions_;
    antennae::InputActionMap* actionMap_;
};

static void GameLogic(PlatformContext& context)
{
    if (context.actions_->IsDown(antennae::InputAction::MOVE_LEFT))
    {
        std::cout << "MoveLeft" << std::endl;
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
    antennae::Keyboard keyboard_;
    antennae::Mouse mouse_;
    antennae::InputActions actions_;
    antennae::InputActionMap actionMap_;
};

Engine::Engine()
    : consoleLogger_(logManager_)
{
}

void Engine::Run()
{
    if (!Init())
    {
        return;
    }

    Loop();

    Shutdown();
}

bool Engine::Init()
{
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

void Engine::Shutdown()
{
    swarm::ShutdownRenderContext(renderContext_);
    swarm::ShutdownSystem();

    terra::ShutdownWindowContext(&windowContext_);
    terra::ShutdownSystem();
}

void Engine::Loop()
{
    PlatformContext platformContext{&renderContext_, &windowContext_, &keyboard_, &mouse_, &actions_, &actionMap_};
    while (!terra::ShouldWindowClose(platformContext.windowContext_))
    {
        terra::PollEvents(platformContext.windowContext_);
        const terra::InputState* input = terra::GetWindowInputState(platformContext.windowContext_);
        antennae::UpdateKeyboard(*platformContext.keyboard_, input);
        antennae::UpdateMouse(*platformContext.mouse_, input);
        antennae::UpdateInputActions(*platformContext.actions_, *platformContext.actionMap_, *platformContext.keyboard_,
                                     *platformContext.mouse_);
        GameLogic(platformContext);
    }
}

int main()
{
    Engine engine;
    engine.Run();
}
