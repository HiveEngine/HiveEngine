# ============================================================================
# Drone - Unified Job System for HiveEngine
# ============================================================================

add_library(Drone STATIC)

hive_configure_target(Drone)

target_sources(Drone
    PRIVATE
        src/drone/drone_module.cpp
        src/drone/coroutine_allocator.cpp
)

target_include_directories(Drone
    PUBLIC
        include
)

target_link_libraries(Drone
    PUBLIC
        Hive
        Comb
        Wax
)
