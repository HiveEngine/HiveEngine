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

#include "core/rendering/Renderer2D.h"

#include "platform/opengl/opengl_shader.h"



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

    setupInput();

    setupEcs();
}

int main()
{
    init();


    hive::OrthographicCamera m_Camera(-1.0f, 1.0f, -1.0f, 1.0f);

    std::string fragmentPath = "../../HiveEngine/assets/shaders/basicColorShader.frag.glsl";
    std::string vertexPath = "../../HiveEngine/assets/shaders/basicColorShader.vert.glsl";

    std::shared_ptr<hive::OpenglShader> colorShader = std::make_shared<hive::OpenglShader>(vertexPath, fragmentPath);

    fragmentPath = "../../HiveEngine/assets/shaders/textureShader.frag.glsl";
    vertexPath = "../../HiveEngine/assets/shaders/textureShader.vert.glsl";

    std::shared_ptr<hive::OpenglShader> textureShader = std::make_shared<hive::OpenglShader>(vertexPath, fragmentPath);

    std::shared_ptr<hive::VertexArray> vertexArray;
    std::shared_ptr<hive::VertexArray> squareVA;

    vertexArray.reset(hive::VertexArray::create());

    float vertices[3 * 7] = {
            -0.5f, -0.5f, -0.1f, 0.8f, 0.2f, 0.8f, 1.0f,
            0.5f, -0.5f, -0.1f, 0.2f, 0.3f, 0.8f, 1.0f,
            0.0f,  0.5f, -0.1f, 0.8f, 0.8f, 0.2f, 1.0f
    };

    std::shared_ptr<hive::VertexBuffer> vertexBuffer = std::shared_ptr<hive::VertexBuffer>(hive::VertexBuffer::create(vertices, sizeof(vertices)));
    hive::BufferLayout layout = {
            { hive::ShaderDataType::Float3, "a_Position" },
            { hive::ShaderDataType::Float4, "a_Color" }
    };
    vertexBuffer->setLayout(layout);

    vertexArray->addVertexBuffer(vertexBuffer);

    uint32_t indices[3] = { 0, 1, 2 };
    std::shared_ptr<hive::IndexBuffer> indexBuffer;
    indexBuffer.reset(hive::IndexBuffer::create(indices, sizeof(indices)));
    vertexArray->setIndexBuffer(indexBuffer);

    squareVA.reset(hive::VertexArray::create());

    float squareVertices[5 * 4] = {
            -0.75f, -0.75f, -0.2f,  0.0f, 0.0f,
            0.75f, -0.75f, -0.2f,  1.0f, 0.0f,
            0.75f,  0.75f, -0.2f,  1.0f, 1.0f,
            -0.75f,  0.75f, -0.2f, 0.0f, 1.0f
    };

    std::shared_ptr<hive::VertexBuffer> squareVB = std::shared_ptr<hive::VertexBuffer>(hive::VertexBuffer::create(squareVertices, sizeof(squareVertices)));
    squareVB->setLayout({
                                {hive::ShaderDataType::Float3, "a_Position"},
                                { hive::ShaderDataType::Float2, "a_TexCoord" }
                        });
    squareVA->addVertexBuffer(squareVB);

    uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
    std::shared_ptr<hive::IndexBuffer> squareIB;
    squareIB.reset(hive::IndexBuffer::create(squareIndices, sizeof(squareIndices)));
    squareVA->setIndexBuffer(squareIB);

    std::shared_ptr<hive::Texture2D> m_Texture = hive::Texture2D::Create("../../Sandbox/assets/textures/Checkerboard.png");
    std::shared_ptr<hive::Texture2D> grassTexture = hive::Texture2D::Create("../../Sandbox/assets/textures/grass.jpg");

    textureShader->bind();
    textureShader->uploadUniformInt("u_Texture", 0);

    float angle = 0.0f;

    hive::Renderer::init();

    //Game loop
    auto window = hive::WindowManager::getCurrentWindow();
    while(!window->shouldClose()) {

        angle += 0.005f;

        m_Camera.setPosition({ 0.5f, 0.0f, 0.0f });
        m_Camera.setRotation(angle);

        hive::Renderer::beginScene(m_Camera);

        m_Texture->bind();
        hive::Renderer::submitGeometryToDraw(squareVA, textureShader);
        hive::Renderer::submitGeometryToDraw(vertexArray, colorShader);

        hive::Renderer2D::beginScene(m_Camera);
        hive::Renderer2D::drawQuad({ 0.0f, -0.5f }, { 1.0f, 0.5f }, { 0.8f, 0.2f, 0.8f, 1.0f });

        hive::Renderer2D::drawQuad({ 0.0f, 0.0f, -0.3f }, { 10.0f, 10.0f }, grassTexture);

        hive::Renderer::endScene();
        hive::Renderer2D::endScene();

        //Swap the buffer
        window->onUpdate();

        //Run all the systems
        hive::ECS::updateSystems(0);
    }

    window.reset();

    shutdown();

    return 0;
}