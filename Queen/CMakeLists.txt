# ============================================================================
# Queen - ECS + Task Scheduler for HiveEngine
# ============================================================================

add_library(Queen ${HIVE_LIB_TYPE})

hive_configure_target(Queen)

target_sources(Queen
    PRIVATE
        src/queen/queen_module.cpp
        src/queen/scheduler_scratch.cpp
)

target_include_directories(Queen
    PUBLIC
        include
)

target_link_libraries(Queen
    PUBLIC
        Hive
        Comb
        Wax
        Drone
)
