# ============================================================================
# Forge - Editor Module (ImGui-based)
# ============================================================================

add_library(Forge STATIC)

# ============================================================================
# ImGui core + backends
# ============================================================================

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG master
)
FetchContent_MakeAvailable(imgui imguizmo)

# Core ImGui sources (compiled into Forge)
set(FORGE_IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)

# Suppress warnings from vendored ImGui code
set_source_files_properties(
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    PROPERTIES
        COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-sign-conversion;-Wno-conversion;-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-double-promotion"
)

# ============================================================================
# Vulkan ImGui backend
# ============================================================================

if(HIVE_FEATURE_VULKAN)
    list(APPEND FORGE_IMGUI_SOURCES ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)

    set_source_files_properties(
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        PROPERTIES
            COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-sign-conversion;-Wno-conversion;-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-double-promotion"
    )
endif()

# ============================================================================
# Forge sources
# ============================================================================

set_source_files_properties(
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
    PROPERTIES
        COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-sign-conversion;-Wno-conversion;-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-double-promotion;-Wno-shadow;-Wno-float-equal"
)

target_sources(Forge
    PRIVATE
        src/forge/imgui_integration.cpp
        src/forge/theme.cpp
        src/forge/selection.cpp
        src/forge/hierarchy_panel.cpp
        src/forge/undo.cpp
        src/forge/inspector_panel.cpp
        src/forge/gizmo.cpp
        src/forge/scene_file.cpp
        src/forge/asset_browser.cpp
        src/forge/toolbar.cpp
        ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
        ${FORGE_IMGUI_SOURCES}
)

target_include_directories(Forge
    PUBLIC
        include
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imguizmo_SOURCE_DIR}
)

target_link_libraries(Forge
    PUBLIC
        Queen
        Swarm
    PRIVATE
        glfw
)

# Vulkan: volk + Vulkan-Headers for imgui_impl_vulkan and imgui_integration
if(HIVE_FEATURE_VULKAN)
    target_link_libraries(Forge PRIVATE volk::volk Vulkan::Headers)
    target_compile_definitions(Forge PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)

    # Platform surface defines
    if(WIN32)
        target_compile_definitions(Forge PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(Forge PRIVATE VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_WAYLAND_KHR)
    endif()
endif()

# Forge includes Diligent headers (via swarm/platform/diligent_swarm.h) which need exceptions + RTTI
target_compile_options(Forge PRIVATE /EHsc /GR)

target_compile_features(Forge PUBLIC cxx_std_20)
