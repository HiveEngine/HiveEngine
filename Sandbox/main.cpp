#include <memory>

#include "core/inputs/input.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "core/window/window.h"
#include "core/window/WindowManager.h"
#include "core/window/window_configuration.h"
#include "core/window/window_factory.h"
#include "scene/components.h"
#include "scene/ECS.h"
#include "scene/query_builder.h"
#include "scene/system_manager.h"

#include "core/rendering/VertexBuffer.hpp"
#include "core/rendering/IndexBuffer.hpp"
#include "core/rendering/VertexArray.hpp"
#include "core/rendering/BufferUtils.h"
#include "core/rendering/Texture.h"
#include "core/rendering/shader.h"
#include "core/rendering/Renderer.hpp"
#include "core/rendering/orthographic_camera.h"
#include "scene/scene.h"

#include "core/Profiling/profiler.h"
#include "core/rendering/orthographic_camera.h"
#include "core/rendering/Texture.h"
#include "GLFW/glfw3.h"
#include "platform/opengl/opengl_shader.h"
#include "stb_image.h"
#include "core/rendering/Renderer2D.h"

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
        if(hive::Input::getKeyDown(hive::KEY_SPACE))
        {
            auto window = hive::WindowManager::getCurrentWindow();
            auto config = window->getConfiguration();

            config.toggle(hive::WindowConfigurationOptions::CURSOR_DISABLED);
            window->updateConfiguration(config);
        }
    }

    hive::OrthographicCamera m_Camera(-1.0f, 1.0f, -1.0f, 1.0f);


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

    float vertices[3 * 7] = {
            -0.5f, -0.5f, -0.1f, 0.8f, 0.2f, 0.8f, 1.0f,
            0.5f, -0.5f, -0.1f, 0.2f, 0.3f, 0.8f, 1.0f,
            0.0f,  0.5f, -0.1f, 0.8f, 0.8f, 0.2f, 1.0f
    };


void shutdown()
{
    hive::ECS::shutdown();
    hive::Input::shutdown();
    hive::WindowManager::setCurrentWindow(nullptr);
    hive::Logger::shutdown();
}

void init()
{
    setupLogger(hive::LogOutputType::Console, hive::LogLevel::Debug);

    hive::WindowConfiguration configuration;
    // configuration.set(hive::WindowConfigurationOptions::CURSOR_DISABLED, true);
    setupWindow(configuration);

    float squareVertices[5 * 4] = {
            -0.75f, -0.75f, -0.2f,  0.0f, 0.0f,
            0.75f, -0.75f, -0.2f,  1.0f, 0.0f,
            0.75f,  0.75f, -0.2f,  1.0f, 1.0f,
            -0.75f,  0.75f, -0.2f, 0.0f, 1.0f
    };

    setupEcs();
}

int main()
{
    init();

    std::shared_ptr<hive::Texture2D> m_Texture = hive::Texture2D::Create("../Sandbox/assets/textures/Checkerboard.png");
    std::shared_ptr<hive::Texture2D> grassTexture = hive::Texture2D::Create("../Sandbox/assets/textures/grass.jpg");

        //Run all the systems
        hive::ECS::updateSystems(0);
    }


    hive::Renderer::init();

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(reinterpret_cast<GLFWwindow*>(window->getNativeWindow())))
    {
    	BLOCK_PROFILING("BLOCK TEST", hive::BlockStatus::ON, hive::colors::Green);
        angle += 0.005f;

    shutdown();

        hive::Renderer::beginScene(m_Camera);

        m_Texture->bind();
        hive::Renderer::submitGeometryToDraw(squareVA, textureShader);
        hive::Renderer::submitGeometryToDraw(vertexArray, colorShader);

        hive::Renderer2D::beginScene(m_Camera);
        hive::Renderer2D::drawQuad({ 0.0f, -0.5f }, { 1.0f, 0.5f }, { 0.8f, 0.2f, 0.8f, 1.0f });

        hive::Renderer2D::drawQuad({ 0.0f, 0.0f, -0.3f }, { 10.0f, 10.0f }, grassTexture);


        hive::Renderer::endScene();
        hive::Renderer2D::endScene();


        /* Poll for and process events */
        window->onUpdate();
    	END_BLOCK_PROFILING;
    }
	DUMP_PROFILING("test.prof");
    hive::Input::shutdown();
    return 0;
}
