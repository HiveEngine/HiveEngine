set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(dep/googletest EXCLUDE_FROM_ALL)

enable_testing()

# Specify the test executable
file(GLOB_RECURSE TEST_SOURCES HiveEngine/*.cpp)
add_executable(maintest MainTest.cpp ${TEST_SOURCES})
target_include_directories(maintest PRIVATE ${CMAKE_SOURCE_DIR}/HiveEngine/src)

target_link_libraries(maintest PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    HiveEngine
)

add_test(NAME maintest COMMAND maintest)

if (NOT TARGET clean_coverage)
    add_custom_target(clean_coverage
            COMMAND ${CMAKE_COMMAND} -E remove *.gcda
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()
