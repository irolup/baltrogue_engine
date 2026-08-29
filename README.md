# Game Engine

Cross-platform game engine for **PS Vita** and **Linux**, with a visual editor on Linux.

## Features

### Rendering
- [x] Basic 2D Rendering
- [x] Basic 3D Rendering
- [x] Skyboxes
- [x] Skeletal Animations (GLTF bone animations)
- [x] GLTF/GLB Model Loading
- [x] PBR (with normal map)
- [x] Multi-light Lighting System (Directional, Point, Spot)
- [x] Custom Shader Support (Lit and Unlit, GLSL + CG templates)
- [x] Vulkan Backend (editor support not yet ported)

### Audio
- [x] 2D Audio
- [ ] 3D Audio

### Physics
- [x] 3D Physics (Bullet Physics with multithreading support on Vita and Linux)
- [ ] 2D Physics

### Scene and Editor
- [x] Scene System (Load/Save JSON scenes)
- [x] Visual Editor (Scene Graph, Viewport, Properties Panel)
- [x] Input Mapping from Editor
- [x] Lua Hot Reloading
- [x] Component System

### Platform Support
- [x] PS Vita Build
- [x] Linux Build

## Architecture

- **Scene Graph**: hierarchy and transform inheritance
- **Component System**: modular entity components
- **Cross-Platform**: shared codebase for Vita and Linux
- **Editor**: scene editing on Linux only

## Visual References

Editor layout (scene graph, viewport, properties):

![Editor Screenshot](docs/references/screenshot_first_demo_v2.png)

First demo scene (physics, player, camera):

![First Demo Scene](docs/references/demo.gif)

## Directory Structure

```
baltrogue_engine/
├── game_engine/           # Core engine
│   ├── include/          # Headers (Core, Scene, Components, Rendering, Input, Editor)
│   └── src/              # Implementation
│       └── App/          # Entry points (game_main, vita_main, editor_main, Platform)
├── assets/
│   ├── scenes/           # Game scenes (shipped; compiled to .bscn for Vita)
│   ├── scripts/          # Game Lua — tire_game/ and ui/ plus the shared menus
│   ├── templates/        # Node templates (prefabs)
│   ├── models/           # Game models
│   ├── textures/         # Shared texture library (recursively scanned)
│   ├── fonts/            # Fonts
│   ├── shaders/          # CG shaders (Vita) + GLSL (Linux OpenGL)
│   ├── linux_shaders/    # OpenGL 2.0 compatible shaders
│   ├── vulkan/           # GLSL sources for Vulkan backend (compiled to .spv)
│   └── samples/          # Engine showcases — never shipped, see samples/README.md
├── config/               # Build settings, input mappings, shadow settings
├── tools/                # scene_to_binary (JSON -> .bscn)
├── vendor/               # Bullet, Lua, ImGui, OpenAL, stb, json
└── Makefile
```


## Engine Samples

`assets/samples/` holds feature showcases. Open them in the editor:

- **samples/scenes/playground.json** : FPS controller, weapons, nav-mesh enemies with
  line-of-sight, physics interactables
- **samples/scenes/first_game_demo.json** : third-person player, collectibles, goal
  volume, GLTF skeletal animation

## Building

### Linux

```bash
make install-deps          # dependencies (libglfw, glew, lua5.3, libvulkan-dev, etc.)
make install-editor-deps   # editor deps (ImGui is in vendor/)
make linux                 # build game (Vulkan enabled by default)
make linux USE_VULKAN=0    # build game without Vulkan (OpenGL only)
make editor                # build editor (OpenGL; no Vulkan)
make run                   # run game
make run-editor            # run editor
make help                  # list all targets
```

Output: `build_linux/Baltrogue`. The executable name and the game's window
title come from the `pc` block of `config/build_settings.txt`, see
[Build names and LiveArea assets](#build-names-and-livearea-assets).

**Vulkan (Linux game build)**

The Linux game target compiles the Vulkan backend by default (`USE_VULKAN=1`). You need:

- **System packages** — installed by `make install-deps` (`libvulkan-dev`, `vulkan-headers`)
- **`glslc`** — from the [Vulkan SDK](https://vulkan.lunarg.com/) (or set `VULKAN_SDK` so `$(VULKAN_SDK)/bin/glslc` is found)

Shaders in `assets/vulkan/` are compiled to SPIR-V (`.spv`) automatically during `make linux`. `make clean` removes generated `.spv` files.

This backend is **work in progress**. It currently renders **meshes** and **text** (screen-space and world-space) only. The OpenGL path still provides the full feature set above; use `USE_VULKAN=0` if you do not have Vulkan installed or want the legacy renderer.

### PS Vita

Requires [VitaSDK](https://vitasdk.org/) and vitaGL. Set up Lua:

```bash
git submodule update --init --recursive
./setup.sh lua
```

Then:

```bash
make vita
```

Output: `build/Baltrogue.vpk`.

### Build names and LiveArea assets

`config/build_settings.txt` holds what each build is called and, for the Vita,
the LiveArea images. Edit it in the editor under **View > Build Settings**, or by
hand, one block per platform, like `config/shadow_settings.txt`:

| Key | Goes into |
|-----|-----------|
| `pc:title` | window title of the game, read at startup (no rebuild) |
| `pc:executableName` | `build_linux/<executableName>` |
| `vita:title`, `vita:titleId`, `vita:appVersion` | `param.sfo` |
| `vita:vpkName` | `build/<vpkName>.vpk` |
| `vita:style`, `vita:icon0`, `vita:pic0`, `vita:bg0`, `vita:startup` | `sce_sys/` |

Each image key points at a source PNG of any size. `scripts/build_livearea.sh`
scales and quantises it into what the Vita expects, writes `template.xml`, and
`make vita` packs the result:

| Source | Becomes | Size | Notes |
|--------|---------|------|-------|
| `icon0` | `sce_sys/icon0.png` | 128 x 128 | app icon, no transparency |
| `pic0` | `sce_sys/pic0.png` | 960 x 544 | fullscreen loading image |
| `bg0` | `sce_sys/livearea/contents/bg0.png` | 840 x 500 | LiveArea "paper" background |
| `startup` | `sce_sys/livearea/contents/startup.png` | 280 x 158 | above the Start button, alpha kept |

An empty key ships the VPK without that asset. The conversion needs `ffmpeg` and
`pngquant`, both installed by `make install-deps`; `make livearea` rebuilds
`sce_sys/` on its own without touching the game build.

To wipe and rebuild only the Vita tree — leaving the Linux and editor builds
untouched — use `make rebuild-vita`. See
[Cleaning and rebuilding one target](#cleaning-and-rebuilding-one-target).

**Running the homebrew**

- **Real Vita**: You need `libshacccg.suprx` on your Vita. See the [Vita troubleshooting guide](https://cimmerian.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx) for how to obtain and install it.
- **Vita3K**: If you use the [Vita3K](https://vita3k.org/) emulator, install or place `libshacccg.suprx` in Vita3K’s data path so the emulator can load it when running the homebrew.

### Dependencies

- **Lua**: submodule, build for Vita with `./setup.sh lua`
- **ImGui**: `./setup.sh imgui /path/to/imgui` for editor
- **All**: `./setup.sh all /path/to/imgui`
- JSON, STB, Bullet, TinyGLTF: included or submodules

### Build targets

| Target | Description |
|--------|--------------|
| `make vita` | PS Vita build |
| `make livearea` | Rebuild `sce_sys/` from `config/build_settings.txt` |
| `make linux` | Linux game (Vulkan on by default) |
| `make linux USE_VULKAN=0` | Linux game without Vulkan |
| `make editor` | Linux editor |
| `make run` | Run Linux game |
| `make run-editor` | Run editor |
| `make debug-linux` / `make debug-editor` | Debug builds |
| `make lua-vita` | Build Lua 5.3 static library for PS Vita |
| `make build-bullet` | Bullet Physics build instructions |
| `make clean` | Clean all build dirs and compiled Vulkan shaders (`.spv`) |
| `make clean-vita` | Clean only `build/` |
| `make clean-linux` | Clean only `build_linux/`, `build_linux_gl/` and `.spv` shaders |
| `make clean-editor` | Clean only `build_editor/` |
| `make clean-scenes` | Remove generated `.bscn` files and `tools/scene_to_binary` |
| `make rebuild-vita` | `clean-vita` then `vita` — other build trees untouched |
| `make rebuild-linux` | `clean-linux` then `linux` |
| `make rebuild-editor` | `clean-editor` then `editor` |
| `make help` | List all targets |

### Cleaning and rebuilding one target

Each target has its own object tree, so you rarely need a full `make clean`:

| Target | Object tree |
|--------|-------------|
| `vita` | `build/` |
| `linux` (`USE_VULKAN=1`) | `build_linux/` |
| `linux USE_VULKAN=0` | `build_linux_gl/` |
| `editor` | `build_editor/` |

To wipe and rebuild just one of them, leaving the others intact:

```bash
make rebuild-vita              # or rebuild-linux / rebuild-editor
make -j$(nproc) rebuild-vita   # parallel
```

The `rebuild-*` targets shell out to a recursive `make` rather than listing clean
and build as prerequisites, so the clean-then-build order still holds under `-j`.

**When a clean is actually required.** Object files do not depend on the
`Makefile`, so changing compiler flags or `-D` defines for a platform will not
recompile that platform — clean its tree first. Adding or removing *source*
files needs no clean; make picks those up on its own.

**Editing the Vita VPK asset list.** The `.vpk` rule depends only on
`eboot.bin`, so adding or changing an `-a` line in the `vita-pack-vpk` command
will not trigger a repack on its own. Force one without recompiling:

```bash
rm -f build/ToTheWell.vpk && make vita
```

**Binary scenes.** `make vita` runs `scene-binaries`, converting
`assets/scenes/*.json` to `.bscn` (except `save_file.json`, which stays JSON).
These live in `assets/`, not in a build dir, so no clean target removes them —
use `make clean-scenes` if the converter itself changed.

## Editor

- **Scene Graph (left)**: select nodes, right-click for context menu
- **Viewport (center)**: preview and manipulation
- **Properties (right)**: transform and component props
- **Create menu**: empty node, camera, cube, lights...
- **View > Build Settings**: game name per platform — Linux executable and window title, VPK name, title id and LiveArea images

## Documentation

- [Full Documentation](docs/DOCUMENTATION.md)
- [Lua API Reference](docs/LUA_API.md)
- [TextComponent API](docs/TEXT_COMPONENT_API.md)
- [ModelRenderer Guide](docs/README_ModelRenderer.md)
