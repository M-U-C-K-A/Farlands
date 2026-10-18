<p align="center">
  <img src="docs/assets/hero.svg" alt="Farlands — a voxel engine built from scratch in C++ and Vulkan" width="100%">
</p>

<p align="center">
  <b>A Minecraft-style voxel engine written from scratch in C++20 and Vulkan, with a physically based HDR renderer.</b><br>
  Sun shadows · atmospheric sky · volumetric clouds & fog · reflective water · shader presets (Realistic, Golden Hour, Horror) · far terrain up to 6 km · falling leaves · multithreaded infinite world
</p>

<p align="center">
  <a href="#roadmap"><img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-e8a95c?style=flat-square"></a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-005fc6?style=flat-square&logo=c%2B%2B&logoColor=white">
  <img alt="Vulkan" src="https://img.shields.io/badge/API-Vulkan-e33a39?style=flat-square&logo=vulkan&logoColor=white">
  <img alt="GLSL to SPIR-V" src="https://img.shields.io/badge/shaders-GLSL%20%E2%86%92%20SPIR--V-7c5cf5?style=flat-square">
  <img alt="Platform: macOS (MoltenVK)" src="https://img.shields.io/badge/platform-macOS%20(MoltenVK)-555?style=flat-square&logo=apple&logoColor=white">
  <img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-6ac9ce?style=flat-square">
</p>

<p align="center">
  <a href="https://github.com/M-U-C-K-A/Farlands/archive/refs/heads/main.zip"><img alt="Download the source code" src="https://img.shields.io/badge/%E2%AC%87%20Download-Source_Code-005fc6?style=for-the-badge"></a>
</p>

<p align="center">
  <a href="#screenshots">Screenshots</a> ·
  <a href="#features">Features</a> ·
  <a href="#quick-start">Quick start</a> ·
  <a href="#how-it-works">How it works</a> ·
  <a href="#project-layout">Project layout</a> ·
  <a href="#roadmap">Roadmap</a> ·
  <a href="#development">Development</a>
</p>

---

## Screenshots

<p align="center">
  <img src="docs/assets/screenshots/lake.jpg" alt="A turquoise lake reflecting a green hill and cherry trees, with a snowy peak and cumulus clouds in the background" width="100%">
</p>

<table>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/cherry.jpg" alt="A cherry grove in full bloom with soft shadows under the canopy" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/mountains.jpg" alt="A snowy rocky peak above spruce forests and grassy slopes, under volumetric clouds" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Cherry grove</b> — alpha-tested foliage, waving leaves, PCF soft shadows</sub></td>
    <td align="center"><sub><b>Mountains</b> — ridged multifractal peaks, snow line, aerial perspective</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/sunset.jpg" alt="Sunset over the lake: warm sky, sun glint on the water, bloom and god rays" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/underwater.jpg" alt="Underwater view: blue-green absorption, sandy lake bed and Snell's window on the surface" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Sunset</b> — sun color from atmospheric transmittance, glint, bloom</sub></td>
    <td align="center"><sub><b>Underwater</b> — Beer–Lambert absorption, Snell's window, caustics</sub></td>
  </tr>
</table>

### Shader presets

<table>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/golden-hour.jpg" alt="Golden Hour preset: low sun behind a snowy peak, golden mist over the lake and light shafts" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/horror.jpg" alt="Horror preset: thick grey fog, desaturated colors and a square sun glowing through the mist" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Golden Hour</b> — low sun, valley mist, volumetric light shafts, warm grading</sub></td>
    <td align="center"><sub><b>Horror</b> — overcast sky, dense fog, square sun, desaturated grading</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/far-terrain.jpg" alt="View from a summit: lakes, cherry groves and distant mountain ranges rendered by the far terrain LOD" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/golden-vista.jpg" alt="Sunset over a valley seen from a summit, with the far terrain stretching to the horizon" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Far terrain</b> — LOD tiles up to 6 km beyond the chunk render distance</sub></td>
    <td align="center"><sub><b>Sunset vista</b> — atmospheric scattering over kilometres of terrain</sub></td>
  </tr>
</table>

<table>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/sand-transition.jpg" alt="Grass fading into sand with an organic pixel-art edge instead of a hard block boundary" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/menu.jpg" alt="Main menu drawn over the live world, slowly panning across the spawn vista" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Surface transitions</b> — grass, sand and snow blend with a pixel-art mask</sub></td>
    <td align="center"><sub><b>Main menu</b> — drawn over the live world</sub></td>
  </tr>
</table>

<p align="center"><sub>Real frames from <code>build/farlands</code> (Release) on an Apple M4 through MoltenVK, 1280×720, render distance 20 chunks — about 90–140 FPS with every effect on.</sub></p>

---

## Features

<table>
  <tr>
    <td valign="top" width="50%">

**🎨 Rendering**
- Hand-written **Vulkan** renderer, HDR (RGBA16F) pass chain, reverse-Z, camera-relative coordinates
- **Atmosphere**: Rayleigh + Mie single scattering (+ multiple-scattering term) baked into a sky LUT every frame
- **Volumetric clouds**: ray-marched cumulus layer (tileable Perlin–Worley 3D noise), half resolution, cloud shadows on the ground
- **Sun shadows**: 4096² shadow map, texel-snapped and cached between frames, PCF with a rotated Poisson disk, alpha-tested foliage
- **Water**: procedural waves, screen-space reflections with an environment fallback, refraction, Beer–Lambert absorption, sun glint, caustics, view from below
- **Terrain shading**: normal maps derived from the textures, sky ambient light, per-vertex AO, foliage translucency, waving leaves and grass, height fog / aerial perspective
- **Volumetric fog**: ray-marched ground mist lit through the shadow map, so sunlight streams between trees and peaks
- **Shader presets**: Realistic, Golden Hour and Horror — sun, clouds, fog, light shafts and color grading in one click
- **Post**: bloom mip chain, god rays, ACES tonemapping, contrast / filter / shadow tint grading, vignette, dithering
- **Far terrain (LOD)**: heightmap tiles generated straight from the world generator, out to 6 km
- **Falling leaves and petals**: instanced particles lit by the sun, landing on the ground or floating on water
- Per-chunk GPU buffers, frustum culling, opaque / cutout / water passes, GPU timers

</td>
    <td valign="top" width="50%">

**🌍 World**
- **Infinite** X/Z world of 16 × 256 × 16 chunks, render distance up to 32 chunks
- **Multithreaded** streaming: a job system generates and meshes chunks in the background, nearest first
- Terrain from Perlin fBm: continents, lakes, **ridged multifractal mountains** with domain warping, snow line and rocky cliffs
- **10 biomes**: Ocean, Beach, Plains, Forest, Desert, Mountains, Taiga, Snowy Taiga, Cherry Grove, Snowy Peaks
- **Trees** (oak, birch, spruce, cherry), tall grass and flowers, ores — deterministic across chunk borders
- **27 data-driven blocks** with biome tints; procedural pixel-art textures for the new ones
- **Smooth surface transitions** between grass, sand and snow, and irregular beach edges
- **Scenic spawn**: the game searches for a spot with a lake in front and peaks in view

</td>
  </tr>
  <tr>
    <td valign="top">

**🧍 Player**
- First-person controller with frame-rate independent movement
- Gravity, jumping, **swimming**, terminal velocity with sub-stepping
- **Per-axis AABB collisions** (0.6 × 1.8 hitbox)

</td>
    <td valign="top">

**🛠️ Tooling**
- **Options** window in tabs: *Shaders* (presets, sky, fog, grading), *Graphics* (distances, shadows, display), *Performance* (FPS, 1% low, frame-time graph, GPU time per pass, CPU breakdown, draw calls, triangles, memory) and *System* (GPU, Vulkan, display, build)
- Optional in-game **FPS counter**, **F3 overlay**, **F11 borderless fullscreen**
- **F2 screenshots**, coloured logger, Vulkan validation layers in Debug
- `terrain_preview` tool: renders a top-down map of the generator

</td>
  </tr>
</table>

---

## Quick start

> [!WARNING]
> **Pre-alpha** — Farlands is under heavy development. Expect breaking changes in the code, the save format and the controls.

**1. Get the code** (Dear ImGui is a git submodule):

```bash
git clone --recursive https://github.com/M-U-C-K-A/Farlands.git
cd Farlands

# Already cloned without --recursive? Fetch ImGui:
git submodule update --init --recursive
```

**2. Install the prerequisites:**

```bash
# macOS (Homebrew) — tested
brew install cmake glfw glm vulkan-headers vulkan-loader molten-vk shaderc
```

```bash
# Debian / Ubuntu — untested, package names may vary
sudo apt install cmake g++ libglfw3-dev libglm-dev libvulkan-dev glslc
```

**3. Build and run:**

```bash
cmake -S . -B build
cmake --build build -j
./build/farlands
```

The main menu is drawn over the live world while it streams in the background; click **Singleplayer** to play from the spawn vista.

### Controls

| Key | Action |
| --- | --- |
| <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> | Move |
| <kbd>Space</kbd> | Jump · swim up |
| Mouse | Look around |
| <kbd>Esc</kbd> | Main menu (Options, Quit) |
| <kbd>Tab</kbd> | Toggle mouse capture |
| <kbd>F2</kbd> | Screenshot → `~/.farlands/screenshots/` |
| <kbd>F3</kbd> | Debug overlay (FPS, GPU passes, chunks, biome) |
| <kbd>F11</kbd> | Borderless fullscreen |

### Performance tips

Everything can be tuned live in **Options**, and the *Performance* tab shows where the time goes. On slower GPUs, lower the **render distance** (vertex load) and the **render scale** (pixel load), or disable **SSR**, **clouds** or **volumetric fog**. The **far terrain** is cheap (a few hundred thousand vertices); shadows are cached and only re-rendered when you move several blocks or change the sun.

---

## How it works

### One frame

The CPU streams the world, moves the player, then records a single command buffer. The scene is rendered off-screen in HDR and composed into the swapchain at the end:

```mermaid
flowchart LR
    A[Sky LUT<br/>256×128] --> B[Sky environment<br/>+ clouds, 512×256]
    B --> C[Clouds<br/>½ resolution]
    C --> D[Shadow map<br/>cached]
    D --> E[Scene<br/>chunks · far terrain · leaves · sky]
    E --> F[Water<br/>SSR · refraction]
    F --> G[Volumetric fog<br/>½ resolution]
    G --> H[Bloom<br/>5 mips]
    H --> I[Composite<br/>god rays · grading · ACES · UI]
```

All passes share one pipeline layout: a global descriptor set (uniforms, texture arrays, shadow map, sky, render targets) and 32 bytes of push constants (the chunk origin relative to the camera).

### Lighting model

Light is expressed in the units of the sky model: the sun reaches the top of the atmosphere with an intensity of **22** and is attenuated by the same Rayleigh/Mie transmittance on the CPU (sun color) and on the GPU (sky radiance). Surfaces are Lambertian:

```glsl
// shaders/terrain_body.glsl (simplified)
vec3 sun     = sunColor * saturate(dot(N, L)) * sunShadow(pos, N) * cloudShadow(world);
vec3 ambient = skyAmbient(N) * ao;                       // a few taps of the sky LUT
vec3 color   = albedo * (sun / PI + ambient);
color        = applyFog(color, pos, distance);           // height fog tinted by the sky
```

The composite pass applies the exposure, ACES tonemapping and saturation. The swapchain is sRGB, so the gamma curve is applied by the hardware.

### Volumetric fog and presets

The fog pass marches each pixel's view ray (at half resolution, 24 steps) up to the visible surface. Density is concentrated near the water level and modulated by drifting noise; each sample receives sunlight through the **shadow map** (forward-scattering phase function), which is what draws light shafts between trees and behind mountains, plus diffuse sky light. The result is upsampled with depth awareness and applied before bloom, so the mist glows around the sun.

A **preset** (`applyPreset()` in `render_settings.h`) only changes the mood — sun elevation, cloud coverage, fog density and height, light shafts and color grading — and leaves performance settings alone. Golden Hour and Horror also place the sun in front of the camera. Under heavy cloud cover the fog, ambient light and sky turn grey (luminance-preserving), and clouds block much more sunlight.

### Far terrain (LOD)

`LodTerrain` covers everything between the chunk render distance and the LOD distance (2 km by default, up to 6 km) with 256-block tiles built on 3 background threads straight from `TerrainGenerator` — no chunk needed. Resolution drops with distance (one vertex every 4, 8 then 16 blocks); vertices carry the biome surface color (grass tint, sand, rock on cliffs, snow, water) and forests are raised into a coloured canopy. Skirts hide the cracks between resolutions, tiles sit slightly below the real terrain and are discarded inside the chunk radius, so the editable world always wins near the player.

### Infinite world streaming

`World::update()` runs on the main thread every frame. When the player enters a new chunk it queues **generation jobs** for every missing chunk within the render distance + 1, sorted by distance, and unloads chunks beyond + 2.5. Worker threads generate chunks; as soon as a chunk and its 8 neighbours exist, a **meshing job** builds its mesh from an immutable 3×3 neighbourhood (`shared_ptr<const Chunk>`). Finished meshes are uploaded to their own GPU buffer, a few dozen per frame; replaced buffers are destroyed once no frame in flight uses them.

### Terrain generation

`TerrainGenerator::getHeightAt()` combines continents, hills and detail fBm, deepens oceans, carves lakes into valleys, and raises mountains with a **ridged multifractal** (Musgrave) behind domain warping, softly compressed below the world ceiling. The biome is chosen from the height (ocean, beach, mountains, snowy peaks) then from temperature × humidity. Surfaces follow the slope: rock on cliffs, snow above the snow line, two layers of grass on medium slopes. Trees are placed on a jittered 5×5 grid hashed from the world seed, so a tree spanning several chunks is identical in each of them.

<p align="center">
  <img src="docs/assets/terrain-map.jpg" alt="Top-down map of 2304 by 2304 blocks rendered by terrain_preview: lakes, forests, cherry groves in pink and snowy mountain ranges" width="70%"><br>
  <sub>Rendered by <code>terrain_preview</code> for seed 42 (1 pixel = 3 blocks).</sub>
</p>

### Meshing

`buildChunkMesh()` emits a face only when the neighbour doesn't hide it, looks into the neighbouring chunks, and computes a per-vertex **ambient occlusion** factor from the two sides and the corner, flipping the quad diagonal to avoid anisotropy. Plants are two crossed double-sided quads, water surfaces are lowered to 7/8 of a block, and the indices are split into **opaque**, **cutout** (leaves, plants, glass) and **water** ranges so that the opaque bulk renders without `discard`.

<p align="center">
  <img src="docs/assets/meshing.svg" alt="Left: cross-section where interior faces are culled. Right: isometric AO demo where blocks appear one by one and the corner darkens from 1 to 0" width="100%">
</p>

A vertex is **20 bytes**: the position inside the chunk, a packed word (texture layer, normal, AO, UV corner, block id, wave flag) and an RGBA8 biome tint.

### Surface transitions and falling leaves

Top faces of grass, sand and snow store, per vertex, the neighbouring surface type and how many of the blocks around that corner use it (4 spare bits of the packed vertex word). The fragment shader turns that into a blend weight and thresholds it with a pixel-art noise (16 texels per block plus large-scale noise); both faces of a boundary use the same field, so the pattern stays continuous across blocks. Beach edges also follow a noise instead of a contour line.

`LeafParticles` probes random blocks around the camera: cherry and broad leaves with air below spawn petals and leaves that tumble in the wind, land on the ground or float on water, then fade out. They are drawn as one instanced draw call with a procedural, pixelated leaf shape, shadows and translucency.

### Blocks

Blocks are data-driven: `BlockDatabase` loads `assets/data/blocks.json` at start-up. Each entry gives its flags, how it is rendered, its biome tint and one texture per face.

<p align="center">
  <img src="docs/assets/blocks.svg" alt="The original 14 visible blocks rendered as textured isometric cubes" width="100%">
</p>

```json
{
  "id": 21,
  "name": "Cherry_Leaves",
  "renderType": "cube",
  "transparent": true,
  "solid": true,
  "hardness": 0.2,
  "tint": "none",
  "waving": true,
  "texture": "textures/block/cherry_leaves.png"
}
```

`renderType` is `cube`, `cross` (plants) or `liquid`; `tint` is `none`, `grass` or `foliage`. The textures of the newer blocks are generated by `scripts/gen_textures.py`.

### Saving

Only chunks modified by the player are written to `~/.farlands/world` (block editing is not implemented yet, so the world is regenerated from the seed). Chunks are stored x-first, then y, then z, as runs of `[type: u8][count: u16]` after a magic number and a version; older or corrupted files are simply regenerated.

<p align="center">
  <img src="docs/assets/chunk-format.svg" alt="Hex dump of a chunk file: magic FLCH, version, chunk coordinates, then RLE runs" width="100%">
</p>

---

## Project layout

```text
Farlands/
├── src/
│   ├── main.cpp                entry point, global exception guard
│   ├── app.{h,cpp}             Application — window, main loop, input, HUD, scenic spawn
│   ├── core/                   logger.h · types.h (Vertex, uniforms) · job_system.* · stb_*
│   ├── renderer/
│   │   ├── renderer.*          pass chain, chunk/LOD/particle buffers, descriptors, GPU timers, screenshots
│   │   ├── pipelines.*         render passes and pipeline builder
│   │   ├── textures.*          block texture arrays (+ normal maps), UI images, cloud noise
│   │   ├── atmosphere.*        sun direction and transmittance (CPU)
│   │   ├── vk_context.*        instance, device, queues, validation layers
│   │   ├── vk_swapchain.*      swapchain, present modes, framebuffers
│   │   ├── vk_utils.*          images, buffers, barriers, samplers
│   │   ├── camera.*            reverse-Z projection, frustum planes
│   │   └── render_settings.h   options and shader presets
│   ├── world/
│   │   ├── world.*             streaming, job scheduling, mesh events
│   │   ├── lod_terrain.*       far terrain tiles (LOD)
│   │   ├── leaf_particles.*    falling leaves and petals
│   │   ├── terrain_generator.* relief, biomes, surface, trees, plants
│   │   ├── chunk.*             16×256×16 block storage + biome tints
│   │   ├── chunk_mesh.*        face culling, AO, plants, water
│   │   ├── chunk_serializer.*  RLE chunk files
│   │   ├── biome.*             biome table
│   │   ├── noise.*             Perlin noise + fBm octaves
│   │   └── block.*             BlockDatabase (JSON)
│   ├── player/player.*         first-person controller, physics, AABB
│   └── ui/                     menu.* (main menu, Options tabs, FPS counter) · debug_info.* (stats)
├── shaders/                    common · atmosphere · lighting (*.glsl) and the pass shaders
├── tools/terrain_preview.cpp   top-down map of the terrain generator
├── scripts/gen_textures.py     procedural textures for the newer blocks
├── assets/                     data/blocks.json · textures/ · Farlands.png (logo)
├── external/                   imgui (submodule) · nlohmann/json.hpp
└── docs/assets/                README visuals and screenshots
```

### Tuning knobs

| What | Where | Default |
| --- | --- | --- |
| Graphics options | `RenderSettings` in `src/renderer/render_settings.h` (or the Options window) | render distance 20, far terrain 2048, shadows 4096², clouds 0.58 |
| Shader presets | `applyPreset()` in `src/renderer/render_settings.h` | Realistic |
| World seed | `m_world{42}` in `src/app.h` | 42 |
| Sea level | `SEA_LEVEL` in `src/world/biome.h` | 62 |
| Snow line | `TerrainGenerator::getSnowLine()` | 156 ± 12 |
| Biomes, tree and plant densities | `src/world/biome.cpp` | — |
| Chunk size | `CHUNK_SIZE_*` in `src/world/chunk.h` | 16 × 256 × 16 |
| Physics | `GRAVITY`, `JUMP_VELOCITY`, `MOVE_SPEED` in `src/player/player.h` | −20 · 8 · 6 |
| Sun intensity / exposure | `SUN_IRRADIANCE` in `renderer.cpp`, `composite.frag` | 22 · 0.5 |

---

## Roadmap

**Done**

- [x] Vulkan bootstrap, MoltenVK portability, validation-clean
- [x] HDR pass chain: atmosphere, volumetric clouds, shadow map, water with SSR, bloom, god rays, ACES
- [x] Multithreaded generation and meshing, one GPU buffer per chunk, frustum culling
- [x] Terrain with lakes, ridged mountains, snow, 10 biomes, trees, grass and flowers
- [x] First-person controller with swimming and AABB collisions
- [x] Main menu over the live world, Options window, F3 overlay with GPU timers, F2 screenshots
- [x] Volumetric fog with light shafts, shader presets (Realistic, Golden Hour, Horror), color grading
- [x] Far terrain LOD up to 6 km, falling leaves, smooth grass / sand / snow transitions
- [x] Performance and system tabs, FPS counter, F11 fullscreen

**Next**

- [ ] Greedy meshing (fewer vertices for shadows and distant terrain)
- [ ] Cascaded shadow maps
- [ ] Breaking and placing blocks (and saving them)
- [ ] Caves
- [ ] Day / night cycle, moon and stars
- [ ] Point lights (lanterns, torches)

---

## Development

You need:

- a C++20 compiler (Clang, GCC or MSVC),
- the Vulkan loader + headers (or the [Vulkan SDK](https://vulkan.lunarg.com/); on macOS, MoltenVK),
- `glslc` (shipped with the Vulkan SDK, or `brew install shaderc`),
- GLFW 3.3+ and GLM.

```bash
# Configure (Release by default)
cmake -S . -B build

# Build the engine and compile the shaders to SPIR-V
cmake --build build -j

# Run the engine
./build/farlands
```

For a debug build with the Vulkan validation layers and the `DEBUG` / `TRACE` logs (`brew install vulkan-validationlayers` on macOS):

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
VK_LAYER_PATH=/opt/homebrew/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d ./build-debug/farlands
```

Preview the terrain generator without starting the engine:

```bash
cmake --build build --target terrain_preview
./build/terrain_preview map.png 1024 0 0 4   # output, size, center x, center z, blocks per pixel
```

- Shaders are recompiled by CMake whenever a `.vert`, `.frag` or included `.glsl` changes. `shaders/compile.sh [out_dir]` does the same by hand.
- Formatting follows the repository's `.clang-format`.
- Asset paths (`ASSETS_DIR`) and shader paths (`SHADER_DIR`) are baked in at configure time, so the binary can be launched from any directory.

---

## Credits

- [Dear ImGui](https://github.com/ocornut/imgui) — UI and debug overlay
- [GLFW](https://www.glfw.org/) — window and input · [GLM](https://github.com/g-truc/glm) — maths
- [nlohmann/json](https://github.com/nlohmann/json) — block database parsing
- [stb](https://github.com/nothings/stb) — `stb_image`, `stb_image_write`, `stb_image_resize2`
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) — Vulkan on macOS
- Techniques: Nishita's sky model, Schneider's Perlin–Worley clouds (*Nubis*), Jimenez's bloom and interleaved gradient noise, Hill's ACES fit, 0fps's voxel AO

Farlands is a personal project, not affiliated with Mojang Studios or Microsoft. Some placeholder textures in `assets/textures/` follow the Minecraft resource-pack layout; they belong to their respective owners and are not covered by this project's license.

## License

Released under the MIT License — created by [M-U-C-K-A](https://github.com/M-U-C-K-A).
