add_library(ELE STATIC
    src/Vulkan/CommandManager.cpp
    src/Vulkan/FrameBuffers.cpp
    src/Vulkan/Pipeline.cpp
    src/Core/Engine.cpp
    src/Vulkan/RenderPass.cpp
    src/Vulkan/SwapChain.cpp
    src/Vulkan/VulkanContext.cpp
    src/Vulkan/UniformBuffer.cpp
    src/Core/Path.cpp
    src/Vulkan/VertexBuffer.cpp
    src/Vulkan/IndexBuffer.cpp
    src/Core/LevelLoader.cpp
    src/Vulkan/DepthBuffer.cpp
    src/Core/Input.cpp
    src/Core/Camera.cpp
    src/Vulkan/Texture.cpp
    src/Vulkan/DynamicVoxelProxy.cpp
    src/Core/stb_impl.cpp
    src/Scene/Material.cpp
    src/Scene/Mesh.cpp
    src/Core/ResourceManager.cpp
    src/Vulkan/VoxelGI.cpp
    src/Vulkan/VoxelizeRenderPass.cpp
    src/Vulkan/Renderer.cpp
    src/Core/tinygltf_impl.cpp
    src/Core/GltfModel.cpp
    src/Vulkan/LightBuffer.cpp
    src/Vulkan/VoxelChunkManager.cpp
    src/Vulkan/ShadowMap.cpp
    src/Vulkan/ShaderLoader.cpp
    src/Vulkan/EnvironmentBaker.cpp
    src/Vulkan/HdrTexture.cpp
    src/Vulkan/CubeTexture.cpp
    src/Vulkan/ComputePipeline.cpp
    src/Vulkan/VoxelTexture3D.cpp
    src/Vulkan/ShadowRenderPass.cpp
    src/Vulkan/HdrTarget.cpp
    src/Vulkan/HdrRenderPass.cpp
    src/Vulkan/BloomTarget.cpp
    src/Vulkan/PostRenderPass.cpp
    src/Vulkan/PointShadowRenderPass.cpp
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
    src/Core/FreeFlyCameraController.cpp
    src/Core/OrbitCameraController.cpp
    src/Physics/PhysicsSystem.cpp
    src/Vulkan/ShadowAtlas.cpp
    src/Audio/AudioSystem.cpp
    src/Core/miniaudio_impl.cpp
    src/Nav/NavMesh.cpp
    src/Nav/GeometryParser.cpp
    src/AI/AIController.cpp
    src/Nav/SimpleNavMeshBuilder.cpp
    src/Vulkan/BoneMatrixBuffer.cpp
    src/Scene/SkeletalGltfLoader.cpp
    src/Vulkan/SkinnedVertexBuffer.cpp
    src/Scene/SkeletalMesh.cpp
    src/Core/ModelLoader.cpp
    src/Core/AnimationLoader.cpp
    src/Scene/Animation.cpp
)

target_sources(ELE PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(ELE PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/tinygltf
    ${joltphysics_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}
    ${ash_rhi_SOURCE_DIR}/include
    ${ash_rhi_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(ELE PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
    Jolt
    rhi
    meshoptimizer
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

foreach(GLSL ${GLSL_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_BINARY_DIR}/game/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/game/shaders"ss
        COMMAND ${GLSLC} -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common" ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Shader compiling ${FILE_NAME}"
    )
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_FILES})