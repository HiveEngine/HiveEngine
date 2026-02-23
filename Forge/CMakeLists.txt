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

    # Force-include volk.h so Vulkan function pointers are available
    set_source_files_properties(
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        PROPERTIES
            COMPILE_OPTIONS "-include;${CMAKE_SOURCE_DIR}/Swarm/extern/volk/volk.h;-Wno-old-style-cast;-Wno-sign-conversion;-Wno-conversion;-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-double-promotion"
    )
endif()

# ============================================================================
# D3D12 ImGui backend
# ============================================================================

if(HIVE_FEATURE_D3D12)
    list(APPEND FORGE_IMGUI_SOURCES ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)

    set_source_files_properties(
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
        PROPERTIES
            COMPILE_OPTIONS "-Wno-old-style-cast;-Wno-sign-conversion;-Wno-conversion;-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-double-promotion;-Wno-cast-align"
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

# Vulkan: volk + headers include paths
if(HIVE_FEATURE_VULKAN)
    target_include_directories(Forge
        PRIVATE
            ${CMAKE_SOURCE_DIR}/Swarm/extern/vulkan-headers/include
            ${CMAKE_SOURCE_DIR}/Swarm/extern/volk
    )

    # Platform surface defines
    if(WIN32)
        target_compile_definitions(Forge PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(Forge PRIVATE VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_WAYLAND_KHR)
    endif()
endif()

# D3D12: system libs
if(HIVE_FEATURE_D3D12)
    target_link_libraries(Forge PRIVATE d3d12.lib dxgi.lib d3dcompiler.lib)
endif()

target_compile_features(Forge PUBLIC cxx_std_20)
