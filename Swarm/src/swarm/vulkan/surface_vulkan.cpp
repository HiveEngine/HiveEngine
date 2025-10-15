#include <swarm/precomp.h>
#include <swarm/surface.h>

#include <swarm/instance.h>
#include "surfacefactory_vulkan.h"

namespace swarm
{
    Surface::Surface(Instance &instance, SurfaceDescription const &description) :
        m_Instance(instance),
        m_Surface(VK_NULL_HANDLE)
    {
        switch (description.type)
        {
            case SurfaceDescription::SessionType::WIN32:
                m_Surface = vk::SurfaceFactory::CreateWin32Surface(instance);
                break;
            case SurfaceDescription::SessionType::X11:
                m_Surface = vk::SurfaceFactory::CreateX11Surface(instance, description.windowId, static_cast<_XDisplay *>(description.windowHandle));
                break;
            case SurfaceDescription::SessionType::WAYLAND:
                m_Surface = vk::SurfaceFactory::CreateWaylandSurface(instance, static_cast<wl_display *>(description.displayHandle), static_cast<wl_surface *>(description.windowHandle));
                break;
            default:
                break;
        }


    }

    Surface::~Surface()
    {
        if (m_Surface != VK_NULL_HANDLE)
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    }
}
