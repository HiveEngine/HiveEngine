#include <memory>

#include "core/inputs/input.h"
#include "core/inputs/keycode.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "core/rendering/Renderer2D.h"
#include "core/window/window.h"
#include "core/window/WindowManager.h"
#include "core/window/window_configuration.h"
#include "core/window/window_factory.h"
#include "scene/components.h"
#include "scene/ECS.h"
#include "scene/query_builder.h"
#include "scene/system_manager.h"



struct myData : hive::IComponent
{
    float x, y;

    std::string toString() override
    {
        return std::string(std::to_string(x) + " " + std::to_string(y));
    }
};

class TestSystem : public hive::System
{

public:
    void update(float deltaTime) override
    {
        if(hive::Input::getKeyDown(hive::KeyCode::KEY_SPACE))
        {
            auto window = hive::WindowManager::getCurrentWindow();
            auto config = window->getConfiguration();

            config.toggle(hive::WindowConfigurationOptions::CURSOR_DISABLED);
            window->updateConfiguration(config);
        }
    }

    void init() override
    {
        auto query = hive::QueryBuilder<myData>();
        for(auto [entity, data] : query.each())
        {
            hive::Logger::log("We have an entity", hive::LogLevel::Info);
        }
    }
};


void setupLogger(const hive::LogOutputType type, const hive::LogLevel level)
{
    hive::Logger::init(hive::LoggerFactory::createLogger(type, level));
}

void setupWindow(const hive::WindowConfiguration configuration)
{
    auto window = std::shared_ptr<hive::Window>(hive::WindowFactory::Create("Hive Engine", 800, 600, configuration));
    hive::WindowManager::setCurrentWindow(window);
}

void setupInput()
{
    auto window = hive::WindowManager::getCurrentWindow();
    hive::Input::init(window->getNativeWindowData());
}

void setupEcs()
{
    //ECS
    hive::ECS::init();

    // auto registry = hive::ECS::getCurrentRegistry();
    // auto entity = registry->createEntity();
    auto entity = hive::ECS::createEntity();
    hive::ECS::addComponent<myData>(entity);

    hive::ECS::registerSystem(new TestSystem(), "TestSystem");
}


void shutdown()
{
    hive::Renderer2D::shutdown();
    hive::ECS::shutdown();
    hive::Input::shutdown();
    hive::WindowManager::setCurrentWindow(nullptr);
    hive::Logger::shutdown();
}

void setupRenderer()
{
    hive::Renderer2D::init();
}

void init()
{
    setupLogger(hive::LogOutputType::Console, hive::LogLevel::Debug);

    hive::WindowConfiguration configuration;
    // configuration.set(hive::WindowConfigurationOptions::CURSOR_DISABLED, true);
    setupWindow(configuration);

    setupInput();

    setupEcs();
    setupRenderer();
}

int main()
{
    init();

    //Game loop
    hive::OrthographicCamera cam(0, 0, 800, 600);
    auto window = hive::WindowManager::getCurrentWindow();
    while(!window->shouldClose()) {

        window->onUpdate();
        //Run all the systems
        hive::ECS::updateSystems(0);

        hive::Renderer2D::beginScene(cam);
        hive::Renderer2D::drawQuad(glm::vec2(0, 0), glm::vec2(20, 20), glm::vec4(255, 1, 1, 255));
        hive::Renderer2D::endScene();

    }

    window.reset();

    shutdown();

    return 0;
}
