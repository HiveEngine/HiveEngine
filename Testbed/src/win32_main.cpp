#include <testbed/precomp.h>

#include <terra/terra.h>
#include <terra/platform/glfw_terra.h>
#define TERRA_NATIVE_WIN32
#include <terra/terra_native.h>

#include <swarm/swarm.h>
#include <swarm/platform/win32_swarm.h>
#include <swarm/platform/diligent_swarm.h>

#include <hive/core/log.h>


#include <iostream>

struct PlatformContext
{
    swarm::RenderContext *renderContext_;
    terra::WindowContext *windowContext_;
};

void GameLogic(PlatformContext &context)
{
    terra::InputState *currentInput = terra::GetWindowInputState(context.windowContext_);

    if (currentInput->keys_[GLFW_KEY_A]) //TODO don't use direct GLFW ID
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

Engine::Engine() : consoleLogger_(logManager_)
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
    if (!terra::InitSystem() || !terra::InitWindowContext(&windowContext_))
    {
        return false;
    }

    if (!swarm::InitSystem())
    {
        return false;
    }

    terra::NativeWindow nativeWindow = terra::GetNativeWindow(&windowContext_);
    if (!swarm::InitRenderContextWin32(&renderContext_, nativeWindow.instance_, nativeWindow.window_))
    {
        return false;
    }

    return true;
}

void Engine::Shutdown()
{
    swarm::ShutdownRenderContext(&renderContext_);
    swarm::ShutdownSystem();

    terra::ShutdownWindowContext(&windowContext_);
    terra::ShutdownSystem();
}

void Engine::Loop()
{
    PlatformContext platformContext{&renderContext_, &windowContext_};
    while (!terra::ShouldWindowClose(platformContext.windowContext_))
    {
        terra::PollEvents();
        GameLogic(platformContext);

        //TODO remove this
        glfwSwapBuffers(platformContext.windowContext_->window_);
    }
}


int main()
{
    Engine engine;
    engine.Run();
}
