cmake_minimum_required(VERSION 3.20)
project(ELE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Engine/Vulkan/CommandManager.cpp
    src/Engine/Vulkan/FrameBuffers.cpp
    src/Engine/Vulkan/Pipeline.cpp
    src/Engine/Vulkan/Renderer.cpp
    src/Engine/Vulkan/RenderPass.cpp
    src/Engine/Vulkan/SwapChain.cpp
    src/Engine/Vulkan/VulkanContext.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
)

find_program(GLSLC glslc REQUIRED)

file(GLOB_RECURSE GLSL_FILES
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/shaders/*.comp"
)

file(GLOB_RECURSE GLSL_FILES
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/shaders/*.comp"
)
 
foreach(GLSL ${GLSL_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT  ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${GLSLC} ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Компиляция шейдера ${FILE_NAME}"
    )
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPIRV_FILES})
add_dependencies(${PROJECT_NAME} shaders)