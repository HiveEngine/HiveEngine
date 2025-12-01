# ============================================================================
# Wax - HiveEngine Standard Library Alternative
# ============================================================================

# ============================================================================
# Library Configuration
# ============================================================================

# Wax is a header-only library (Array, Span, Vector, etc.)
add_library(Wax INTERFACE)

target_include_directories(Wax
    INTERFACE
        include
)

target_link_libraries(Wax
    INTERFACE
        Hive
        Comb
)

target_compile_features(Wax
    INTERFACE
        cxx_std_20
)
