# ============================================================================
# Nectar - Asset Pipeline for HiveEngine
# ============================================================================

add_library(Nectar STATIC)

file(GLOB_RECURSE NECTAR_SOURCES src/nectar/*.cpp)

target_sources(Nectar
    PRIVATE
        ${NECTAR_SOURCES}
)

target_include_directories(Nectar
    PUBLIC
        include
)

target_link_libraries(Nectar
    PUBLIC
        Hive
        Comb
        Wax
)

target_compile_features(Nectar
    PUBLIC
        cxx_std_20
)