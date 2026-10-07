find_package(Threads REQUIRED)
add_executable(ele_texconv main.cpp)
target_include_directories(ele_texconv PRIVATE ${CMAKE_SOURCE_DIR}/external/stb)
target_link_libraries(ele_texconv PRIVATE ktx Threads::Threads)
