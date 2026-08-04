add_library(ELE STATIC
    src/Vulkan/CommandManager.cpp
    src/Vulkan/FrameBuffers.cpp
    src/Vulkan/Pipeline.cpp
    src/Core/Engine.cpp
    src/Vulkan/RenderPass.cpp
    src/Vulkan/SwapChain.cpp
    src/Vulkan/VulkanContext.cpp
    src/Vulkan/Shader.cpp
    src/Vulkan/UniformBuffer.cpp
    src/Core/Path.cpp
    src/Vulkan/VertexBuffer.cpp
    src/Vulkan/IndexBuffer.cpp
    src/Vulkan/DepthBuffer.cpp
    src/Core/Input.cpp
    src/Core/Camera.cpp
    src/Vulkan/Texture.cpp
    src/Core/stb_impl.cpp
    src/Scene/Material.cpp
    src/Scene/Mesh.cpp
    src/Core/ResourceManager.cpp
    src/Vulkan/Renderer.cpp
    src/Core/tinygltf_impl.cpp
    src/Core/GltfModel.cpp
    src/Vulkan/LightBuffer.cpp
    src/Vulkan/PipelineCache.cpp
    src/Vulkan/ShadowMap.cpp
    src/Vulkan/ShadowRenderPass.cpp
    src/Vulkan/HdrTarget.cpp
    src/Vulkan/HdrRenderPass.cpp
    src/Vulkan/BloomTarget.cpp
    src/Vulkan/PostRenderPass.cpp
    src/Vulkan/PointShadowRenderPass.cpp
    src/Vulkan/CubeShadowMap.cpp
    src/Vulkan/BloomChain.cpp
    src/Vulkan/LdrTarget.cpp
    src/Vulkan/FXAA.cpp
    src/Vulkan/VelocityTarget.cpp
    src/Vulkan/HistoryBuffer.cpp
    src/Vulkan/DecalBox.cpp
    src/Scene/ScreenSpaceDecal.cpp
    src/Vulkan/DecalCameraBuffer.cpp
    src/Vulkan/FogCameraBuffer.cpp
    src/Vulkan/GodRayTarget.cpp
    src/Core/TimeSystem.cpp
    src/Core/GLFWGamepad.cpp
)

target_include_directories(ELE PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/tinygltf
)

target_link_libraries(ELE PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
)

target_compile_definitions(ELE PUBLIC
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    TINYGLTF_NO_STB_IMAGE_WRITE
)

find_program(GLSLC glslc REQUIRED)

file(GLOB_RECURSE GLSL_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
)

set(SPIRV "${CMAKE_BINARY_DIR}/game/shaders/${FILE_NAME}.spv")

foreach(GLSL ${GLSL_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${GLSLC} -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common" ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Shader compiling ${FILE_NAME}"
    )
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_FILES})