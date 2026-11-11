# ELE

A custom Vulkan game engine with hybrid (hardware/software) ray tracing, written in C++20.

## Features

- **Rendering**: Vulkan renderer with a deferred PBR pipeline, skinned meshes, decals, particles, bloom, HDR/tonemapping, TAA/FXAA, volumetric fog, god rays.
- **Ray tracing**: hardware RT passes for shadows, reflections, GI probes and AO, denoised with NVIDIA's NRD (optional, `ELE_USE_NRD`).
- **Physics**: [Jolt Physics](https://github.com/jrouwe/JoltPhysics) (rigid bodies, triggers, character controller).
- **Audio**: [miniaudio](https://github.com/mackron/miniaudio).
- **Scene/ECS**: `GameObject` + component system (`Scene/`, `Components/`), with scene serialization (`Scene/SceneSerializer.h`) and a property reflection system (`Scene/Property.h`, `PropertyIO.h`).
- **Scripting**: game logic can be built as hot-loadable modules (`game/scripts/`, `Scripting/`), loaded as shared objects.
- **Navigation/AI**: navmesh building (`Nav/`) and a command-based AI controller (`AI/`).(needs tests)
- **Tools**: in-tree texture-to-KTX2 converter (`tools/texconv`) and a model optimizer (`tools/emodel_optimizer`), both run automatically at build time.

## Project layout

```
Engine/          engine library (ELE)
  include/       public headers, organized by subsystem (Vulkan, RT, Scene, Physics, Audio, AI, Nav, UI, Scripting, Components, Core)
  src/           implementation
  shaders/       GLSL sources (pbr, rt, voxel, post, shadow, ibl, fx, common), compiled to SPIR-V at build time
  assets/        engine-provided default assets (models, textures, HDRIs)
game/            the game executable, linked against ELE
  src/           game entry point and AI command implementations
  scripts/       game-specific scripts (compiled into the game; same sources load as modules in the editor)
  include/       game-specific components
  assets/        game assets (models, HDRIs, audio, decals)
external/        third-party submodules (glm, glfw, stb, tinygltf, jolt)
tools/           build-time asset tools (texconv, emodel_optimizer)
```

Everything else (ASH-RHI, miniaudio, meshoptimizer, imgui, NRD) is pulled in via CMake `FetchContent`.

## Requirements

- CMake 3.20+
- A C++20 compiler (Clang or GCC)
- Vulkan SDK (loader, headers, validation layers) with a driver that supports ray tracing extensions for the RT path
- Git (for submodules)
- Linux: ALSA, X11/Wayland, and `xkbcommon` dev packages. Windows: VulkanSDK(Windows support needs tests).

## Building

```bash
git submodule update --init --recursive

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

This also compiles all shaders (`Shaders` target) and converts textures to KTX2 as part of the build.

## Running

```bash
./build/game/game
```

or, on Nix (to pick up ALSA from the Nix store at runtime):

```bash
./run.sh
```

## Notes

- `pipeline_cache.bin` and `imgui.ini` at the repo root are runtime-generated state (Vulkan pipeline cache / ImGui layout) and will change on every run — see `.gitignore`.
- `ELE_USE_NRD` (default `ON`, in `Engine/CMakeLists.txt`) controls whether the NVIDIA Real-Time Denoiser is built in for the RT passes.
