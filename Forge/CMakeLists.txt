add_library(Forge STATIC)

hive_configure_target(Forge RTTI ON EXCEPTIONS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set_target_properties(Forge PROPERTIES AUTOMOC ON)

target_sources(Forge
    PRIVATE
        src/forge/theme.cpp
        src/forge/selection.cpp
        src/forge/hierarchy_panel.cpp
        src/forge/undo.cpp
        src/forge/editor_undo.cpp
        src/forge/inspector_panel.cpp
        src/forge/material_inspector.cpp
        include/forge/material_inspector.h
        src/forge/asset_browser.cpp
        src/forge/console_panel.cpp
        src/forge/toolbar.cpp
        src/forge/scene_file.cpp
        src/forge/forge_module.cpp
        src/forge/forge_main_window.cpp
        src/forge/progress_overlay.cpp
        src/forge/project_hub.cpp
        src/forge/vulkan_viewport_widget.cpp
        include/forge/hierarchy_panel.h
        include/forge/console_panel.h
        include/forge/project_hub.h
        include/forge/vulkan_viewport_widget.h
        include/forge/inspector_panel.h
        include/forge/asset_browser.h
        include/forge/toolbar.h
        include/forge/forge_main_window.h
        include/forge/progress_overlay.h
)

target_include_directories(Forge
    PUBLIC
        include
)

target_precompile_headers(Forge
    PRIVATE
        include/forge/precomp.h
)

if(WIN32)
    target_compile_definitions(Forge PRIVATE NOMINMAX)
endif()

target_link_libraries(Forge
    PUBLIC
        Queen
        Swarm
        Qt6::Widgets
    PRIVATE
        Waggle
        glfw
)

if(UNIX AND NOT APPLE)
    find_package(Fontconfig REQUIRED)
    target_link_libraries(Forge PRIVATE Fontconfig::Fontconfig)
endif()

if(HIVE_FEATURE_VULKAN)
    target_link_libraries(Forge PRIVATE volk::volk Vulkan::Headers)

    if(WIN32)
        target_compile_definitions(Forge PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(Forge PRIVATE VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_WAYLAND_KHR)
    endif()
endif()
