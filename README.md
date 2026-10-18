<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/hero-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/hero-light.svg">
    <img src="docs/assets/hero-light.svg" alt="Farlands — a voxel engine built from scratch in C++ and Vulkan, next to an isometric island cut from the real seed-42 world" width="100%">
  </picture>
</p>

<p align="center">
  <b>A Minecraft-style voxel engine written from scratch in C++20 and Vulkan, with a physically based HDR renderer.</b><br>
  Sun & moon shadows · atmospheric sky · volumetric clouds & fog · reflective water · shader presets (Realistic, Golden Hour, Horror, Night) · large walled villages with churches, forges and harbors · temples & other structures · amplified peaks · flying & building · lanterns and torches that light the world · far terrain up to 6 km · multithreaded infinite world
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

### Villages

<p align="center">
  <img src="docs/assets/screenshots/village.jpg" alt="A walled town on a lakeshore: dozens of half-timbered houses with red tile roofs, a church with a bell tower, crenellated ramparts with towers, a windmill and fields outside the walls, piers on the lake" width="100%">
</p>

<table>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/village-night.jpg" alt="The same walled town at night, lanterns glowing on the towers, along the streets and in the windows" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/village-church.jpg" alt="Inside the church: stone walls, pews made of stairs, a red carpet down the aisle, stained-glass windows, a rose window and a golden altar" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>At night</b> — towers, streets and windows lit by lanterns</sub></td>
    <td align="center"><sub><b>Church</b> — stained glass, rose window, pews, altar</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/village-harbor.jpg" alt="A harbor below the town walls: a stone quay, plank piers on log posts with lanterns, and sailboats moored alongside" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/village-plaza.jpg" alt="The village square: patterned cobblestone paving, a covered well, lamp posts and market stalls, houses with stair-built roofs around" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Harbor</b> — quay, piers, moored boats when a street reaches the water</sub></td>
    <td align="center"><sub><b>Square</b> — paving, well or fountain, market stalls, lamp posts</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/village-windmill.jpg" alt="A windmill with white canvas sails outside the town wall, next to the fields" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/village-taiga.jpg" alt="A snowy taiga town with spruce houses and slate roofs inside its ramparts, a frozen pond and forest around" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Outside the walls</b> — windmills, barns with pens, open fields along the roads</sub></td>
    <td align="center"><sub><b>Taiga style</b> — spruce, slate roofs (plains, taiga, cherry, snowy, desert styles)</sub></td>
  </tr>
</table>

### Structures, amplified peaks and nights

<table>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/desert-temple.jpg" alt="A stepped sandstone pyramid with two obelisks at the edge of a desert dotted with cacti" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/shrine.jpg" alt="A cherry shrine at golden hour: pavilion with a tiered roof, vermilion torii gates and stone lanterns" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Desert temple</b> — stepped pyramid with a pillared hall, obelisks, cacti</sub></td>
    <td align="center"><sub><b>Cherry shrine</b> — torii gates, stone lanterns, tiered roof (Golden Hour)</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/lighthouse-night.jpg" alt="A striped white and red lighthouse at night, its lantern room glowing above the forest" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/ruins-horror.jpg" alt="A crumbling mossy stone tower with cobwebs, half hidden in thick grey fog" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Lighthouse</b> — on the beaches, its lantern room glows at night</sub></td>
    <td align="center"><sub><b>Ruined tower</b> — mossy bricks, rubble and cobwebs (Horror)</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/amplified.jpg" alt="Amplified Peaks: needle-like mountains covered in grassy ledges and spruces, snow on the highest tips" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/amplified-village.jpg" alt="A village at the bottom of a deep valley between towering cliffs in an amplified world" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Amplified Peaks</b> — towering spires, grassy ledges, 3D overhangs</sub></td>
    <td align="center"><sub><b>Amplified world</b> — the whole map amplified, villages in the valleys</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/building.jpg" alt="A small lakeside gazebo built by the player with stone bricks, logs, spruce planks, a lantern and torches, the hotbar at the bottom" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/building-night.jpg" alt="The same gazebo at night, lit by its lantern and the torches on the pier" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Building</b> — break, place and pick blocks from a 9-slot hotbar</sub></td>
    <td align="center"><sub><b>Block light</b> — lanterns and torches light their surroundings</sub></td>
  </tr>
</table>

<table>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/block-shapes.jpg" alt="Block shapes: roof-tile stairs in four orientations and slabs, clear and stained-glass panes, an oak fence, a door in a plaster wall" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/sand-transition.jpg" alt="Grass fading into sand with an organic pixel-art edge instead of a hard block boundary" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Block shapes</b> — stairs, slabs, glass panes, stained glass, fences, doors</sub></td>
    <td align="center"><sub><b>Surface transitions</b> — grass, sand and snow blend with a pixel-art mask</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/assets/screenshots/step-blend.jpg" alt="One-block steps between grass and sand: the sides of the grass blocks fade down into the sand below" width="100%"></td>
    <td width="50%"><img src="docs/assets/screenshots/world-tab.jpg" alt="The World tab of the options: teleport buttons for every structure and biome, fly speed, and a new world form with a seed and an amplified option" width="100%"></td>
  </tr>
  <tr>
    <td align="center"><sub><b>Steps</b> — the sides of a one-block step fade into the surface below</sub></td>
    <td align="center"><sub><b>World tab</b> — teleport to the nearest structure or biome, new seed, amplified world</sub></td>
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
- **Shader presets**: Realistic, Golden Hour, Horror and Night — sun, clouds, fog, light shafts and color grading in one click
- **Night**: the moon lights the scene through the same atmosphere model (blue sky glow, moon shadows), twinkling stars, a square moon with the square sun
- **Block light**: lanterns, torches and jack-o'-lanterns emit light that spreads Minecraft-style (15 levels, blocked by solid blocks), smoothed per vertex; their bright texels glow and bloom
- **Post**: bloom mip chain, god rays, ACES tonemapping, contrast / filter / shadow tint grading, vignette, dithering
- **Far terrain (LOD)**: heightmap tiles generated straight from the world generator, out to 6 km
- **Falling leaves and petals**: instanced particles lit by the sun, landing on the ground or floating on water
- Per-chunk GPU buffers, frustum culling, opaque / cutout / water passes, GPU timers

</td>
    <td valign="top" width="50%">

**🌍 World**
- **Infinite** X/Z world of 16 × 256 × 16 chunks, render distance up to 48 chunks
- **Multithreaded** streaming: a job system generates and meshes chunks in the background, nearest first
- Terrain from Perlin fBm: continents, lakes, **ridged multifractal mountains** with domain warping, snow line and rocky cliffs
- **11 biomes**: Ocean, Beach, Plains, Forest, Desert, Mountains, Taiga, Snowy Taiga, Cherry Grove, Snowy Peaks and **Amplified Peaks** (towering spires with 3D-noise overhangs)
- **Amplified world type**: the whole map amplified, with the usual climates in the valleys
- **Large villages** (radius 80–130 blocks, up to ~70 buildings) in 5 regional styles: winding streets and alleys around a paved square with a fountain or well and market stalls; houses of three sizes (one or two storeys, timber framing, stair-built roofs, glass windows, doors, furniture); a **church** with a bell tower, spire, stained glass and pews; a **forge**, a **tavern**, a **library**; **ramparts** with towers and gates for the bigger ones; **windmills**, barns with pens and open **fields** of wheat, carrots, potatoes and beetroots along the outer roads; a **harbor** with a quay, piers and sailboats when a street reaches the water
- **Other structures**: desert temples, ruined towers, igloos, cherry shrines with torii gates, lighthouses
- **Trees** (oak, birch, spruce, cherry), boulders, cacti, dead bushes, pumpkins, tall grass and flowers, ores — deterministic across chunk borders
- **112 data-driven blocks** with biome tints and light levels — including **stairs** (4 orientations), **slabs**, see-through **glass panes**, **stained glass**, **fences** and **doors**; procedural pixel-art textures for the new ones
- **Smooth surface transitions** between grass, sand and snow — on top faces and down the sides of steps — and irregular beach edges
- **Any seed**: generate a new world (normal or amplified) from the World tab
- **Scenic spawn**: the game searches for a spot with a lake in front and peaks in view

</td>
  </tr>
  <tr>
    <td valign="top">

**🧍 Player**
- First-person controller with frame-rate independent movement
- Gravity, jumping, **swimming**, **sprinting**, terminal velocity with sub-stepping
- **Creative flight** (double-tap Space or F) with smooth acceleration, adjustable speed
- **Building**: break, place and pick blocks (DDA raycast, outline on the targeted block), 9-slot hotbar; stairs face the way you look, doors are placed and broken in pairs; edits are saved per world
- **Per-axis AABB collisions** against the real block shapes (slabs, stairs, 1.5-block fences, open doors), automatic step-up onto slabs and stairs, zoom (C)

</td>
    <td valign="top">

**🛠️ Tooling**
- **Options** window in tabs: *Shaders* (presets, sky, fog, grading), *Graphics* (distances, shadows, display), *Performance* (FPS, 1% low, frame-time graph, GPU time per pass, CPU breakdown, draw calls, triangles, memory) and *System* (GPU, Vulkan, display, build)
- *World* tab: teleport (flying) to the nearest village, temple, ruin, igloo, shrine, lighthouse or biome; new world from a seed, amplified option
- Optional in-game **FPS counter**, **F3 overlay**, **F11 borderless fullscreen**, **F1** to hide the HUD
- **F2 screenshots**, coloured logger, Vulkan validation layers in Debug
- **Capture scripts** (`FARLANDS_SCRIPT`): teleport, set presets, edit blocks and take screenshots — how the pictures above were made
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

The main menu is drawn over the live world while it streams in the background; click **Singleplayer** to play from the spawn vista, or **World...** to jump straight to a village, a temple or the Amplified Peaks.

<p align="center">
  <img src="docs/assets/screenshots/menu.jpg" alt="Main menu drawn over the live world, slowly panning across the spawn vista" width="80%">
</p>

### Controls

| Key | Action |
| --- | --- |
| <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> | Move |
| <kbd>Space</kbd> | Jump · swim up · fly up |
| <kbd>Ctrl</kbd> | Sprint · fly faster |
| <kbd>Space</kbd> <kbd>Space</kbd> or <kbd>F</kbd> | Toggle flying (land to stop) |
| <kbd>Shift</kbd> | Fly down |
| Mouse | Look around |
| Left / right click | Break / place a block |
| Middle click | Pick the targeted block into the hotbar |
| <kbd>1</kbd>–<kbd>9</kbd> · wheel | Choose the hotbar slot |
| <kbd>C</kbd> | Zoom |
| <kbd>Esc</kbd> | Main menu (Options, World, Quit) |
| <kbd>Tab</kbd> | Toggle mouse capture |
| <kbd>F2</kbd> | Screenshot → `~/.farlands/screenshots/` |
| <kbd>F3</kbd> | Debug overlay (FPS, GPU passes, chunks, biome) |
| <kbd>F11</kbd> | Borderless fullscreen |
| <kbd>F1</kbd> | Hide the HUD |

### Performance tips

Everything can be tuned live in **Options**, and the *Performance* tab shows where the time goes. On slower GPUs, lower the **render distance** (vertex load) and the **render scale** (pixel load), or disable **SSR**, **clouds** or **volumetric fog**. The **far terrain** is cheap (a few hundred thousand vertices); shadows are cached and only re-rendered when you move several blocks or change the sun.

---

## How it works

### One frame

The main thread streams the world, moves the player and records a single command buffer, while worker threads build chunks and far-terrain tiles. The scene is rendered off-screen in HDR and composed into the swapchain at the end:

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/frame-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/frame-light.svg">
    <img src="docs/assets/frame-light.svg" alt="One frame: main thread (input, world and LOD update, uploads, leaves and culling, record), worker threads (job queue, chunk generation, meshing, far-terrain tiles) and the seven GPU passes from sky to composite" width="100%">
  </picture>
</p>

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

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/streaming-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/streaming-light.svg">
    <img src="docs/assets/streaming-light.svg" alt="Animated top-down grid: chunks are queued, generated then meshed nearest first by the worker threads, and dropped beyond the unload radius as the player walks east" width="100%">
  </picture>
</p>

### Terrain generation

`TerrainGenerator::getHeightAt()` combines continents, hills and detail fBm, deepens oceans, carves lakes into valleys, and raises mountains with a **ridged multifractal** (Musgrave) behind domain warping, softly compressed below the world ceiling. The biome is chosen from the height (ocean, beach, mountains, snowy peaks) then from temperature × humidity. Surfaces follow the slope: rock on cliffs, snow above the snow line, two layers of grass on medium slopes. Trees are placed on a jittered 5×5 grid hashed from the world seed, so a tree spanning several chunks is identical in each of them.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/terrain-slice-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/terrain-slice-light.svg">
    <img src="docs/assets/terrain-slice-light.svg" alt="Pixel-perfect cross-section of 640 columns of the seed-42 world at z = -672: lake, taiga, beach, plains, desert and a snowy mountain, with the sea level and the snow line" width="100%">
  </picture>
</p>

<p align="center">
  <img src="docs/assets/terrain-map.jpg" alt="Top-down map of 2304 by 2304 blocks rendered by terrain_preview: lakes, forests, cherry groves in pink and snowy mountain ranges" width="70%"><br>
  <sub>Rendered by <code>terrain_preview</code> for seed 42 (1 pixel = 3 blocks).</sub>
</p>

### Meshing

`buildChunkMesh()` emits a face only when the neighbour doesn't hide it, looks into the neighbouring chunks, and computes a per-vertex **ambient occlusion** factor from the two sides and the corner, flipping the quad diagonal to avoid anisotropy. Plants are two crossed double-sided quads, water surfaces are lowered to 7/8 of a block, and the indices are split into **opaque**, **cutout** (leaves, plants, glass) and **water** ranges so that the opaque bulk renders without `discard`.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/meshing-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/meshing-light.svg">
    <img src="docs/assets/meshing-light.svg" alt="Face visibility rules on a cross-section with water and a tree, an animated per-vertex AO demo going from level 0 to 3, and the bit layout of the 20-byte vertex" width="100%">
  </picture>
</p>

A vertex is **16 bytes**: the position inside the chunk in 16-bit fixed point (1/64 block) with texture coordinates in 1/16 block, a packed word (texture layer, normal, AO, block id, wave flag, surface blend) and an RGBA8 biome tint whose alpha byte carries the block light level.

### Surface transitions and falling leaves

Top faces of grass, sand and snow store, per vertex, the neighbouring surface type and its share of the blocks around that corner, in quarters (5 spare bits of the packed vertex word). The sides of a one-block step get the same treatment: their top edge matches the top face, their bottom edge the complement seen from the surface below, so the pattern runs down the step without a seam. The fragment shader turns that into a blend weight and thresholds it with a pixel-art noise (16 texels per block plus large-scale noise); both faces of a boundary use the same field, so the pattern stays continuous across blocks. Beach edges also follow a noise instead of a contour line.

`LeafParticles` probes random blocks around the camera: cherry and broad leaves with air below spawn petals and leaves that tumble in the wind, land on the ground or float on water, then fade out. They are drawn as one instanced draw call with a procedural, pixelated leaf shape, shadows and translucency.

### Structures

Villages live on a 640 × 640 grid, the other structures on a 192 × 192 one; each region hashes the seed into at most one **site** — a jittered point whose biome picks the structure (village style, desert temple, ruined tower, igloo, cherry shrine or lighthouse) after a few terrain checks (dry land, gentle slopes, sea nearby for a lighthouse). Sites are cheap to evaluate. A site's **blueprint** — every block, with a mode: *set*, *fill* (foundations only replace air, water or plants), *clear* (interiors) or *soft clear* (plants above paths) — is computed once from the heightmap (the first thread builds it, the others wait for it), bucketed by chunk and cached; each chunk then stamps its own bucket. Buildings are written in local coordinates with their door on −Z and rotated to face their street; oriented blocks (stairs, doors) are rotated with them.

A **village** is decided on an occupancy grid before any block is written: the square, then 3 to 5 main streets that meander out of it (a random walk that stops at water or steep slopes), alleys branching off them, and building slots on both sides of every street. Landmarks take the slots nearest the square (church, forge, tavern, library), then large and medium houses, then small ones fill the gaps — each building tries a few setbacks and sideways shifts until its footprint (plus a one-block gap) is free and flat enough for its foundations. Big villages get a ring of **ramparts** first: the town packs inside, towers stand every ~30 blocks, and gates open wherever a street crosses. Fields, windmills and barns go along the roads outside; a street that ends in the water becomes a **harbor**. The occupied cells, widened by three blocks, become a mask that keeps trees and boulders out of the streets and gardens.

### Amplified terrain

`amplifiedMask()` marks rare regions (or the whole world) where the heightmap is replaced by a sharper, taller ridged multifractal, still softly compressed below the world ceiling. On slopes, a 3D Perlin density (`overhangDensity()`) adds and removes stone around the surface — overhangs, arches, notches in the cliffs — then the surface layers are rebuilt under every opening, so ledges get grass and dirt. Flat valleys and plateaus keep the plain heightmap, which keeps villages and trees on solid ground. The far terrain uses the heightmap only.

### Block light and nights

Blocks with a `light` level in `blocks.json` (lantern, jack-o'-lantern 15, torch 14) are listed per chunk. When a chunk is meshed, the light of every source within 15 blocks — in the chunk or its 8 neighbours — is flood-filled through non-opaque blocks, losing one level per block; each vertex averages the cells around its corner (smooth lighting) and stores the level in the alpha byte of its tint. The fragment shader turns it into warm light (`0.8^(15 − level)`) and adds an emissive term on the bright texels of the light blocks. The **Night** preset swaps the sun for the moon: same direction maths, same atmosphere LUT and shadow map, about 5 % of the intensity with a cool tint, plus stars and a moon disc in the sky pass.

### Block shapes

Stairs, slabs, panes, fences and doors are lists of axis-aligned boxes (`block_shape.h`) shared by the mesher and the collisions. The mesher emits each box face unless it lies on the block boundary against an opaque cube, skips the faces hidden inside a stair, and derives texture coordinates from the position inside the block, so a half face shows half a texture. Panes and fences connect to their neighbours (other panes or fences, glass, opaque cubes); stairs and doors carry their orientation in their block id (four consecutive ids per material).

### Building

The targeted block comes from a voxel DDA (Amanatides & Woo) along the view ray. `World::setBlockAt()` edits the chunk in place, or a copy if a meshing job still holds it, and marks it modified; edits are grouped and the chunk and its neighbours are remeshed once, ahead of the streaming queue (a version number discards stale results).

### Blocks

Blocks are data-driven: `BlockDatabase` loads `assets/data/blocks.json` at start-up. Each entry gives its flags, how it is rendered, its biome tint and one texture per face.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/blocks-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/blocks-light.svg">
    <img src="docs/assets/blocks-light.svg" alt="All 26 visible blocks with their real textures: cubes, cutout leaves and glass, cross-shaped plants and the lowered water block" width="100%">
  </picture>
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

`renderType` is `cube`, `cross` (plants), `liquid`, `stairs`, `slab`, `pane`, `fence` or `door`; `tint` is `none`, `grass` or `foliage`; an optional `light` (0–15) makes the block a light source, `facing` (0–3) orients stairs and doors and `doorTop` marks the upper half of a door. The textures of the newer blocks are generated by `scripts/gen_textures.py` and `scripts/gen_structure_textures.py`:

<p align="center">
  <img src="docs/assets/structure-blocks.png" alt="The 21 structure blocks: stone bricks, mossy bricks and cobblestone, bricks, sandstone, chiseled sandstone, spruce, cherry and vermilion planks, white plaster, lantern, torch, jack-o'-lantern, pumpkin, hay bale, farmland, wheat, dirt path, cactus, dead bush and cobweb" width="80%">
</p>

<p align="center">
  <img src="docs/assets/village-blocks.png" alt="Village blocks: roof tiles, slate, see-through glass, red, blue and yellow stained glass, bookshelf, barrel, forge, crafting table, gold block, oak door, five wools and the carrot, potato, beetroot and wheat crops" width="80%"><br>
  <sub>Village blocks — most also come as stairs, slabs, panes or fences.</sub>
</p>

### Saving

Only chunks modified by the player are written — when they unload and on exit — to `~/.farlands/world` for the default world (seed 42) and to `~/.farlands/worlds/<seed>[-amplified]` for the others; everything else is regenerated from the seed. Chunks are stored x-first, then y, then z, as runs of `[type: u8][count: u16]` after a magic number and a version; older or corrupted files are simply regenerated.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/chunk-format-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/chunk-format-light.svg">
    <img src="docs/assets/chunk-format-light.svg" alt="Hex dump of a real chunk file: magic FLCH, version 3, chunk coordinates, then RLE runs; a 65 536-block chunk stored in 5 590 bytes" width="100%">
  </picture>
</p>

---

## Project layout

```text
Farlands/
├── src/
│   ├── main.cpp                entry point, global exception guard
│   ├── app.{h,cpp}             Application — window, main loop, input, HUD, hotbar, scenic spawn, teleports
│   ├── app_script.cpp          capture scripts (FARLANDS_SCRIPT)
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
│   │   ├── terrain_generator.* relief, amplified overhangs, biomes, surface, trees, boulders, plants
│   │   ├── structures.*        structure sites, blueprint cache (temples, ruins, igloos, shrines, lighthouses)
│   │   ├── village.*           large villages: streets, houses, church, forge, tavern, ramparts, fields, harbor
│   │   ├── structure_builder.h rotated local builder, oriented blocks, deterministic RNG
│   │   ├── block_shape.h       boxes of stairs, slabs, panes, fences, doors (mesh + collisions)
│   │   ├── chunk.*             16×256×16 block storage + biome tints
│   │   ├── chunk_mesh.*        face culling, AO, block light, surface blends, plants, water
│   │   ├── chunk_serializer.*  RLE chunk files
│   │   ├── biome.*             biome table
│   │   ├── noise.*             Perlin noise + fBm octaves
│   │   └── block.*             BlockDatabase (JSON)
│   ├── player/player.*         first-person controller, physics, AABB
│   └── ui/                     menu.* (main menu, Options tabs, FPS counter) · debug_info.* (stats)
├── shaders/                    common · atmosphere · lighting (*.glsl) and the pass shaders
├── tools/terrain_preview.cpp   top-down map of the terrain generator
├── scripts/                    gen_textures.py · gen_structure_textures.py (procedural block textures)
├── assets/                     data/blocks.json · textures/ · Farlands.png (logo)
├── external/                   imgui (submodule) · nlohmann/json.hpp
└── docs/assets/                README visuals and screenshots
```

### Tuning knobs

| What | Where | Default |
| --- | --- | --- |
| Graphics options | `RenderSettings` in `src/renderer/render_settings.h` (or the Options window) | render distance 20, far terrain 2048, shadows 4096², clouds 0.58 |
| Shader presets | `applyPreset()` in `src/renderer/render_settings.h` | Realistic |
| World seed / type | `FARLANDS_SEED`, `FARLANDS_AMPLIFIED=1`, or Options › World | 42, normal |
| Structure density | `REGION`, `VILLAGE_REGION`, `computeSite()`, `computeVillageSite()` in `src/world/structures.cpp` | ≤ 1 per 192² / 640² region |
| Village size and layout | `src/world/village.cpp` (`plan()`) | radius 80–130 |
| Amplified regions | `amplifiedMask()` in `src/world/terrain_generator.cpp` | rare |
| Block light | `light` in `blocks.json`, `BLOCK_LIGHT_INTENSITY` in `shaders/terrain_body.glsl` | 15 · 2.6 |
| Flight | `FLY_SPEED`, `FLY_SPRINT_SCALE` in `src/player/player.h` (Options › World › Fly speed) | 14 · ×3 |
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
- [x] Structures: villages in 5 styles, desert temples, ruined towers, igloos, cherry shrines, lighthouses
- [x] Amplified Peaks biome and amplified world type, 3D overhangs; boulders, cacti, pumpkins
- [x] Flying, sprinting, zoom; breaking and placing blocks with a hotbar, saved per world
- [x] Block light (lanterns, torches), Night preset with moon, stars and moon shadows
- [x] World tab: teleport to structures and biomes, new world from any seed
- [x] Large villages: streets, three house sizes, church, forge, tavern, library, ramparts, windmills, fields, harbors
- [x] Stairs, slabs, glass panes, stained glass, fences, doors (16-byte vertices with fractional UVs, shape collisions)

**Next**

- [ ] Greedy meshing (fewer vertices for shadows and distant terrain)
- [ ] Cascaded shadow maps
- [ ] Caves
- [ ] Animated day / night cycle
- [ ] Villagers and animals
- [ ] Upside-down stairs, opening doors; structures in the far terrain

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

Capture scripts drive the engine for screenshots and visual checks — one command per line (`wait`, `waitload`, `play`, `tp`, `look`, `fly`, `locate`, `biome`, `preset`, `set`, `setblock`, `world`, `hud`, `shot`, `stats`, `quit`; see `src/app_script.cpp`). Worlds opened by a script are never saved.

```bash
cat > /tmp/tour.txt <<'END'
hud 0
play
locate village
waitload
shot village.png
preset night
wait 2
shot village-night.png
quit
END
FARLANDS_SCRIPT=/tmp/tour.txt ./build/farlands
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
