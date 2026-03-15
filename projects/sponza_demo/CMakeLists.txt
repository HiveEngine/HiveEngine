add_library(sponza_demo MODULE src/gameplay.cpp)

hive_configure_target(sponza_demo)

target_link_libraries(sponza_demo PRIVATE Waggle)

set_target_properties(sponza_demo PROPERTIES
    OUTPUT_NAME "gameplay"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

foreach(_config IN ITEMS Debug Profile Retail Release RelWithDebInfo)
    string(TOUPPER "${_config}" _config_upper)
    set_target_properties(sponza_demo PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${_config_upper}" "${CMAKE_CURRENT_SOURCE_DIR}"
        "RUNTIME_OUTPUT_DIRECTORY_${_config_upper}" "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endforeach()
