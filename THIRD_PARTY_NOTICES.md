# Third-party notices

The Apache-2.0 `LICENSE` in this repository covers only the original ELE
source code (`Engine/`, `game/`, `tools/`, build configuration). Everything
below is a separate dependency, pulled in via git submodule (`external/`) or
CMake `FetchContent`, and remains under its own license. Nothing in this
repository's license is applied to, or re-licenses, any of it.

## Git submodules (`external/`)

| Dependency | License | Notes |
|---|---|---|
| [glm](https://github.com/g-truc/glm) | MIT (Happy Bunny License) | full text: `external/glm/copying.txt` |
| [glfw](https://github.com/glfw/glfw) | Zlib/libpng | full text: `external/glfw/LICENSE.md` |
| [stb](https://github.com/nothings/stb) | MIT / Public Domain (dual, your choice) | full text: `external/stb/LICENSE` |
| [tinygltf](https://github.com/syoyo/tinygltf) | MIT | full text: `external/tinygltf/LICENSE` |
| [JoltPhysics](https://github.com/jrouwe/JoltPhysics) | MIT | full text: `external/jolt/LICENSE` |

## Fetched at build time (CMake `FetchContent`)

| Dependency | License | Notes |
|---|---|---|
| [miniaudio](https://github.com/mackron/miniaudio) | Public Domain (Unlicense) / MIT-0 (dual, your choice) | not vendored; downloaded into `build/_deps` |
| [ASH-RHI](https://github.com/CucusinaMullki/ASH-RHI) | MIT | same author as this project; not vendored |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | MIT | not vendored |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT | not vendored |

Full license text for each of the above is included in its own repository
and gets downloaded alongside the source into `build/_deps/<name>-src/` —
not duplicated here to avoid transcription drift; see the links.

## NVIDIA Real-Time Denoiser (NRD) — special case

- Repository: https://github.com/NVIDIA-RTX/NRD
- License: **NVIDIA RTX SDKs License** (proprietary — this is *not* an OSS
  license like MIT/Apache; see the full text at the URL above or in
  `LICENSE.txt` inside the fetched `build/_deps/nrd-src/`).
- Integration: fetched via `FetchContent` in `Engine/CMakeLists.txt` and
  statically linked into `ELE` when `ELE_USE_NRD` is `ON` (default). No NRD
  source is committed to this repository.

Required attribution (NVIDIA RTX SDKs License §2(b)):

> This software contains source code provided by NVIDIA Corporation.

Key restrictions to keep in mind if you build on or redistribute this
project (not an exhaustive restatement — read the actual license before
relying on this):

- The SDK may only be distributed as object code, embedded in an
  application with "material additional functionality" beyond the SDK
  itself (an engine/game, not a thin NRD wrapper) — §2(a).
- You may not distribute or sublicense NRD as a stand-alone product, reverse
  engineer it, or strip its copyright notices — §4(a)-(b).
- You may not do anything that would cause NRD to become subject to an
  open-source license (e.g. vendoring its source into a repo licensed under
  MIT/GPL/Apache and applying that license to it) — §4(e). This is why NRD
  is fetched at build time instead of being committed here, and why the
  Apache-2.0 `LICENSE` in this repo is explicitly scoped to exclude it.
- You may not claim the application is sponsored or endorsed by NVIDIA
  without a separate agreement — §4(c).
- Any NVIDIA trademark use is governed by the Marketing section of the
  license (§6 of the RTX Supplement) — don't use NVIDIA/RTX marks on
  store pages, trailers, etc. without following those terms.

If you disable `ELE_USE_NRD` at configure time (`-DELE_USE_NRD=OFF`), none
of the above applies — the engine falls back to running without NRD.
