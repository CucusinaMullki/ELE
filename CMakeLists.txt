cmake_minimum_required(VERSION 3.20)
project(ELE LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_library(ELEEngine STATIC
    Engine/src/Vulkan/CommandManager.cpp
    Engine/src/Vulkan/FrameBuffers.cpp
    Engine/src/Vulkan/Pipeline.cpp
    Engine/src/Vulkan/Renderer.cpp
    Engine/src/Vulkan/RenderPass.cpp
    Engine/src/Vulkan/SwapChain.cpp
    Engine/src/Vulkan/VulkanContext.cpp
    Engine/src/Vulkan/Shader.cpp
    Engine/src/Vulkan/UniformBuffer.cpp
    Engine/src/Core/Path.cpp
    Engine/src/Vulkan/VertexBuffer.cpp
)

target_include_directories(ELEEngine PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/Engine/include
    ${CMAKE_SOURCE_DIR}/Engine/include/glm 
)

target_link_libraries(ELEEngine PUBLIC
    Vulkan::Vulkan
    glfw
)

find_program(GLSLC glslc REQUIRED)

add_compile_definitions(GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)

file(GLOB_RECURSE GLSL_FILES
    "${CMAKE_SOURCE_DIR}/Engine/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/Engine/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/Engine/shaders/*.comp"
)

foreach(GLSL ${GLSL_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT  ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${GLSLC} ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Shader compiling ${FILE_NAME}"
    )
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_FILES})

add_executable(game
    game/src/main.cpp
)

target_link_libraries(game PRIVATE ELEEngine)
add_dependencies(game Shaders)