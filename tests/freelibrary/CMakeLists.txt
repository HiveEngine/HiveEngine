cmake_minimum_required(VERSION 3.20)
project(freelibrary_test CXX)

set(CMAKE_CXX_STANDARD 20)

# Minimal DLL that links hive_engine
add_library(minimal_test MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/minimal_dll.cpp
)
hive_configure_target(minimal_test)
target_link_libraries(minimal_test PRIVATE hive_engine)
set_target_properties(minimal_test PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Test EXE — must link hive_engine so it stays loaded when gameplay DLL is freed
add_executable(test_freelibrary
    ${CMAKE_CURRENT_SOURCE_DIR}/test_freelibrary.cpp
)
target_link_libraries(test_freelibrary PRIVATE hive_engine)
set_target_properties(test_freelibrary PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
