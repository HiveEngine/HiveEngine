//
// Created by Justin Abourjaili-Bilodeau on 8/19/24.
//
#include "engine.h"

#include <filesystem>
#include "glad/glad.h"

#include "core/logging/Logger.h"
#include "core/logging/LoggingFactory.h"
#include "core/engine/argument_parser.h"

#include "core/window/window_configuration.h"
#include "core/window/window_factory.h"
#include "core/inputs/input.h"
#include "core/window/window.h"

namespace hive {
    Engine::Engine(const int& argc, char *argv[]) : argc(argc), argv(argv) {
        try {
            init();
        } catch (const std::invalid_argument& e) {
            std::cerr << "Failed to initialize engine (invalid_argument): " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        } catch (const std::out_of_range& e) {
            std::cerr << "Failed to initialize engine (out_of_range): " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        } catch (const std::logic_error& e) {
            std::cerr << "Failed to initialize engine (logic_error): " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize engine (exception): " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }

        Logger::log("Engine has successfully initialized", LogLevel::Info);
        Logger::log("Currently running: " + std::filesystem::path(argv[0]).filename().string(), LogLevel::Info);
    }


    Engine::~Engine() {

    }

    void Engine::init() {
        ArgumentParser parser = ArgumentParser(argc, argv, "-", true);
        auto debugArg = parser.addArgument("debug", 0);
        auto testArg = parser.addArgument("test", 1, "int");
        parser.parseArguments();

        // Checking --debug or -d for Logger
        if (parser.checkArgument(debugArg)) {
            Logger::setLogger(LoggingFactory::createLogger(LogOutputType::Console, LogLevel::Debug));
        } else {
            Logger::setLogger(LoggingFactory::createLogger(LogOutputType::Console, LogLevel::Info));
        }

        Logger::log("This should only print if the debug argument was given:", LogLevel::Debug);

        if (parser.checkArgument(testArg)) {
            auto values = parser.getIntValues(testArg);
            for (const auto& value : values) {
                Logger::log(std::to_string(value), LogLevel::Debug);
            }
        }

        // Init Window
        WindowConfiguration configuration;
        configuration.set(WindowConfigurationOptions::CURSOR_DISABLED, true);
        window = std::unique_ptr<Window>(WindowFactory::Create("Hive Engine", 800, 600, configuration));

        // Init Input
        hive::Input::init(window->getNativeWindow());
    }

    void Engine::mainLoop() {
        while (!window->shouldClose()) {
            /* Render here TEMP */
            glClear(GL_COLOR_BUFFER_BIT);
            /* Poll for and process events */
            window->onUpdate();
        }
    }

    void Engine::run() {
        mainLoop();
    }

} // hive
