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

add_library(ELE STATIC
    Engine/src/Vulkan/CommandManager.cpp
    Engine/src/Vulkan/FrameBuffers.cpp
    Engine/src/Vulkan/Pipeline.cpp
    Engine/src/Core/Engine.cpp
    Engine/src/Vulkan/RenderPass.cpp
    Engine/src/Vulkan/SwapChain.cpp
    Engine/src/Vulkan/VulkanContext.cpp
    Engine/src/Vulkan/Shader.cpp
    Engine/src/Vulkan/UniformBuffer.cpp
    Engine/src/Core/Path.cpp
    Engine/src/Vulkan/VertexBuffer.cpp
    Engine/src/Vulkan/IndexBuffer.cpp
    Engine/src/Vulkan/DepthBuffer.cpp
    Engine/src/Core/Input.cpp
    Engine/src/Core/Camera.cpp
    Engine/src/Vulkan/Texture.cpp
    Engine/src/Core/stb_impl.cpp
    Engine/src/Scene/Material.cpp
    Engine/src/Scene/Mesh.cpp
    Engine/src/Core/ResourceManager.cpp
    Engine/src/Vulkan/Renderer.cpp
    Engine/src/Core/tinygltf_impl.cpp
    Engine/src/Core/GltfModel.cpp
    Engine/src/Vulkan/LightBuffer.cpp
    Engine/src/Vulkan/PipelineCache.cpp
    Engine/src/Vulkan/ShadowMap.cpp
    Engine/src/Vulkan/ShadowRenderPass.cpp
)

target_include_directories(ELE PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/Engine/include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/tinygltf
)

target_link_libraries(ELE PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm 
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
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${GLSLC} -I "${CMAKE_SOURCE_DIR}/Engine/shaders/common" ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Shader compiling ${FILE_NAME}"
    )
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_FILES})

add_executable(game
    game/src/main.cpp
)
target_link_libraries(game PRIVATE ELE)
add_dependencies(game Shaders)

target_compile_definitions(ELE PUBLIC
    TINYGLTF_NO_STB_IMAGE_WRITE
)

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Engine/assets
        $<TARGET_FILE_DIR:game>/assets
)