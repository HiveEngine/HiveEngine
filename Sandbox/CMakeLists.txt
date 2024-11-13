
add_executable(Sandbox main.cpp)
target_include_directories(Sandbox PRIVATE ${CMAKE_SOURCE_DIR}/HiveEngine/src)
target_link_libraries(Sandbox HiveEngine)

add_executable(MainInput InputDemo/mainInput.cpp)
target_include_directories(MainInput PRIVATE ${CMAKE_SOURCE_DIR}/HiveEngine/src)
target_link_libraries(MainInput HiveEngine)