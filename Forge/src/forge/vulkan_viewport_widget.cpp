#include <forge/vulkan_viewport_widget.h>

#include <swarm/swarm.h>

#include <terra/terra.h>

#ifdef Q_OS_WIN
#define TERRA_NATIVE_WIN32
#include <terra/terra_native.h>
#endif

#include <QVBoxLayout>
#include <QWindow>

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

#ifdef TERRA_NATIVE_WIN32
        auto native = terra::GetNativeWindow(window);
        if (native.m_window == nullptr)
            return;

        auto* foreignWindow = QWindow::fromWinId(reinterpret_cast<WId>(native.m_window));
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
