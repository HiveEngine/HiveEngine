#include <forge/vulkan_viewport_widget.h>

#include <swarm/swarm.h>
#include <terra/platform/glfw_terra.h>

#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace forge
{
    VulkanViewportWidget::VulkanViewportWidget(QWidget* parent)
        : QWidget{parent}
    {
        setMinimumSize(320, 240);
    }

    void VulkanViewportWidget::EmbedGlfwWindow(terra::WindowContext* window)
    {
        if (window == nullptr)
            return;

#ifdef Q_OS_WIN
        GLFWwindow* glfwWin = terra::GetGlfwWindow(window);
        if (glfwWin == nullptr)
            return;

        HWND hwnd = glfwGetWin32Window(glfwWin);
        if (hwnd == nullptr)
            return;

        auto* foreignWindow = QWindow::fromWinId(reinterpret_cast<WId>(hwnd));
        m_embedded = QWidget::createWindowContainer(foreignWindow, this);

        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_embedded);
#else
        (void)window;
#endif
    }

    void VulkanViewportWidget::SetRenderContext(swarm::RenderContext* ctx)
    {
        m_ctx = ctx;
    }

    void VulkanViewportWidget::RenderFrame()
    {
        if (m_ctx == nullptr)
            return;

        swarm::DrawPipeline(m_ctx);
    }
} // namespace forge
