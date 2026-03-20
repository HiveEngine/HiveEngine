#include <terra/terra.h>

#include <antennae/actions.h>
#include <antennae/input.h>

#include <testbed/precomp.h>

#define TERRA_NATIVE_LINUX
#include <hive/core/log.h>

#include <swarm/swarm.h>

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

    terra::WindowContext* windowContext_{nullptr};
    swarm::RenderContext* renderContext_{nullptr};
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
    if (!terra::InitSystem())
    {
        return false;
    }

    windowContext_ = terra::CreateWindowContext("Hive Engine", 1280, 720);
    if (windowContext_ == nullptr)
    {
        return false;
    }

    if (!swarm::InitSystem())
    {
        return false;
    }

    renderContext_ = swarm::CreateRenderContext(windowContext_);
    if (renderContext_ == nullptr)
    {
        return false;
    }

    swarm::SetupGraphicPipeline(*renderContext_);
    return true;
}

void Engine::Shutdown()
{
    if (renderContext_ != nullptr)
    {
        swarm::DestroyRenderContext(renderContext_);
        renderContext_ = nullptr;
    }
    swarm::ShutdownSystem();

    if (windowContext_ != nullptr)
    {
        terra::DestroyWindowContext(windowContext_);
        windowContext_ = nullptr;
    }
    terra::ShutdownSystem();
}

void Engine::Loop()
{
    PlatformContext platformContext{renderContext_, windowContext_, &keyboard_, &mouse_, &actions_, &actionMap_};
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
