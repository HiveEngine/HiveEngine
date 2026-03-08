# ============================================================================
# Antennae - Input Module
# ============================================================================

add_library(Antennae STATIC)

hive_configure_target(Antennae)

target_sources(Antennae
    PRIVATE
        src/antennae/input.cpp
)

target_include_directories(Antennae
    PUBLIC
        include
)

target_link_libraries(Antennae
    PUBLIC
        Terra
        Queen
        Hive
)
