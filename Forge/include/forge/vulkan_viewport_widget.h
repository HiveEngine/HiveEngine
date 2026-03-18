#pragma once

#include <QWidget>

namespace terra
{
    struct WindowContext;
}

namespace swarm
{
    struct RenderContext;
}

namespace forge
{
    class VulkanViewportWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit VulkanViewportWidget(QWidget* parent = nullptr);

        void EmbedGlfwWindow(terra::WindowContext* window);
        void SetRenderContext(swarm::RenderContext* ctx);
        void RenderFrame();

    private:
        swarm::RenderContext* m_ctx{nullptr};
        QWidget* m_embedded{nullptr};
    };
} // namespace forge
