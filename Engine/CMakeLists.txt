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
    src/Vulkan/Particles.cpp
    src/Vulkan/BoneMatrixBuffer.cpp
    src/Scene/SkeletalGltfLoader.cpp
    src/RT/RTContext.cpp
    src/RT/Blas.cpp
    src/RT/RTWorld.cpp
    src/RT/RTSkinPass.cpp
    src/RT/RTShadowPass.cpp
    src/RT/RTReflectionPass.cpp
    src/Vulkan/SkinnedVertexBuffer.cpp
    src/Scene/SkeletalMesh.cpp
    src/Core/ModelLoader.cpp
    src/RT/SkinnedBlas.cpp
    src/RT/RTNrd.cpp
    src/RT/RTDenoiser.cpp
    src/Core/AnimationLoader.cpp
    src/RT/RTProbeUpdate.cpp
    src/Scene/ProbeSystem.cpp
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

# NVIDIA Readl-Time Denoiser
option(ELE_USE_NRD "Build with NVIDIA Real-Time Denoiser" ON)
if (ELE_USE_NRD)
    include(FetchContent)

    set(NRD_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
    set(NRD_STATIC_SPIRV_SHADERS ON CACHE BOOL "" FORCE)
    set(NRD_STATIC_DXBC_SHADERS OFF CACHE BOOL "" FORCE)
    set(NRD_STATIC_DXIL_SHADERS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        NRD
        GIT_REPOSITORY https://github.com/NVIDIA-RTX/NRD.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(NRD)

    target_link_libraries(ELE PUBLIC NRD)
    target_compile_definitions(ELE PUBLIC ELE_HAS_NRD=1)
endif()

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
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.rgen"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.rmiss"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.rahit"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.rchit"
)

foreach(GLSL ${GLSL_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_BINARY_DIR}/game/shaders/${FILE_NAME}.spv")

    set(TARGET_ENV "")
    if(GLSL MATCHES "\\.(rgen|rmiss|rchit|rahit)$" OR GLSL MATCHES "/shaders/rt/")
        set(TARGET_ENV --target-env=vulkan1.3)
    endif()

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/game/shaders"
        COMMAND ${GLSLC} ${TARGET_ENV} -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common" ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Shader compiling ${FILE_NAME}"
    )
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_FILES})

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(KTX QUIET IMPORTED_TARGET ktx)
endif()

if(TARGET PkgConfig::KTX)
    message(STATUS "libktx: using system library")
    target_link_libraries(ELE PUBLIC PkgConfig::KTX)
else()
    message(STATUS "libktx: fetching KTX-Software")
    include(FetchContent)
    set(KTX_FEATURE_TOOLS         OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TESTS         OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_DOC           OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        ktx_software
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.4.0
    )
    FetchContent_MakeAvailable(ktx_software)
    target_link_libraries(ELE PUBLIC ktx)
endif()