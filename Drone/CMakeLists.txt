# ============================================================================
# Drone - Unified Job System for HiveEngine
# ============================================================================

add_library(Drone ${HIVE_LIB_TYPE})

hive_configure_target(Drone)

target_sources(Drone
    PRIVATE
        src/drone/drone_module.cpp
        src/drone/coroutine_allocator.cpp
        src/drone/service_thread.cpp
        src/drone/worker_context.cpp
        src/drone/parallel_scratch.cpp
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
