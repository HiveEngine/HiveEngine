//
// Created by GuillaumeIsCoding on 7/26/2024.
//
#include "windows_window.h"

namespace platform
{
        WindowsWindow::WindowsWindow(const std::string& title, const uint32_t& width, const uint32_t& height, const core::WindowFlags& flag) noexcept : WindowsWindow(core::WindowProperties(title, width, height, flag)) {}
        
        WindowsWindow::WindowsWindow(const core::WindowProperties& properties) noexcept
        {
            initialize(properties);
        }

        WindowsWindow::~WindowsWindow() noexcept
        {
            shutdown();
        }

        void WindowsWindow::initialize(const core::WindowProperties& properties) noexcept
        {
            data_.title = properties.title;
            data_.width = properties.width;
            data_.height = properties.height;

            int result = glfwInit();

            switch (properties.flag)
            {
                    case core::WindowFlags::FULLSCREEN:
                        monitor_ = glfwGetPrimaryMonitor();
                    case core::WindowFlags::DEFAULT:
                        window_ = glfwCreateWindow(data_.width, data_.height, data_.title.c_str(), monitor_, NULL);
                        break;
                    case core::WindowFlags::WINDOWED_FULLSCREEN:
                        window_ = glfwCreateWindow(data_.width, data_.height, data_.title.c_str(),monitor_, NULL);
                        monitor_ = glfwGetPrimaryMonitor();
                        mode_ = glfwGetVideoMode(monitor_);
                        glfwSetWindowMonitor(window_, monitor_, 0, 0, mode_->width, mode_->height, mode_->refreshRate);
                        break;
            }

            if (!window_)
            {
                std::cout << "Error while initializing the window" << std::endl;
                return;
            }

            glfwMakeContextCurrent(window_);

            if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
            {
                std::cout << "Error in glad load" << std::endl;
                return;
            }


            /* Creates InputManager */
            inputManager_ = Lypo::GlfwInputManager(window_);
            /* Set the key callback */
            glfwSetKeyCallback(window_, inputManager_.key_callback);

            glfwSetWindowUserPointer(window_, &data_);
            setVSync(true);
        }

        void WindowsWindow::shutdown() noexcept
        { 
            glfwDestroyWindow(window_);
            glfwTerminate();
        }

        void WindowsWindow::onUpdate()
        {
            /* Swap front and back buffers */
            glfwSwapBuffers(window_);

            // if(inputManager_.isKeyPressed(67))
            //     std::cout << "key 67 pressed" << std::endl;
            // if(inputManager_.isKeyReleased(67))
            //     std::cout << "key 67 released" << std::endl;
            glfwPollEvents();
        }

        void WindowsWindow::setVSync(bool enabled)
        {
            glfwSwapInterval(enabled ? 1 : 0);
            data_.vSync = enabled;
        }

        bool WindowsWindow::isVSync() const 
        {
            return data_.vSync;
        }

        uint32_t WindowsWindow::getWidth() const 
        {
            return data_.width;
        }

        uint32_t WindowsWindow::getHeight() const 
        {
            return data_.height;
        }

        void* WindowsWindow::getNativeWindow() const
        {
            return window_;
        }
}
