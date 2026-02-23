# ============================================================================
# Antennae - Input Module
# ============================================================================

add_library(Antennae STATIC)

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

target_compile_features(Antennae PUBLIC cxx_std_20)
