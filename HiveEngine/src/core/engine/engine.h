//
// Created by Justin Abourjaili-Bilodeau on 8/19/24.
//

#ifndef LYPO_ENGINE_ENGINE_H
#define LYPO_ENGINE_ENGINE_H

#include "argument_parser.h"
#include "core/window/window_factory.h"

namespace hive {

    class Engine {
    public:
        Engine(const int& argc, char *argv[]);
        ~Engine();

        void run();

    private:
        void init();
        void mainLoop();

        std::unique_ptr<Window> window;
        int argc = 0;
        char **argv;
    };

} // hive

#endif //LYPO_ENGINE_ENGINE_H
