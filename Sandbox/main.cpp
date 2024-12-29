#include <core/Logger.h>
#include <core/Application.h>

#include <rendering/Renderer.h>

#include <iostream>



void RegisterDefaultLoggerSync(hive::Logger::LogLevel level)
{
    hive::Logger::AddSync(level, [](hive::Logger::LogLevel level, const char* msg)
    {
       std::cout << msg << std::endl;
    });
}

int main()
{
    hive::Logger logger;
    RegisterDefaultLoggerSync(hive::Logger::LogLevel::DEBUG);

    hive::ApplicationConfig config{};

    config.log_level = hive::Logger::LogLevel::DEBUG;
    config.window_config.width = 1080;
    config.window_config.height = 920;
    config.window_config.title = "Hive Engine";
    config.window_config.type = hive::WindowConfig::WindowType::GLFW;

    hive::Application app(config);
    app.run();


}
