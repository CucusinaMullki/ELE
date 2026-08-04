cmake_minimum_required(VERSION 3.20)
project(ELE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

add_subdirectory(external/glm)

option(USE_SYSTEM_GLFW "Use system GLFW instead of submodule" OFF)

if(USE_SYSTEM_GLFW)
    find_package(glfw3 REQUIRED)
else()
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/glfw)
endif()

add_subdirectory(Engine)
add_subdirectory(game)