# Baltrogue Engine Documentation

Complete documentation for the Baltrogue Game Engine - a cross-platform engine supporting PS Vita and Linux.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Build System](#build-system)
3. [Scripting Guide](#scripting-guide)
4. [Input System](#input-system)
5. [Menu System](#menu-system)
6. [Components](#components)
7. [Editor Guide](#editor-guide)
8. [Setup Instructions](#setup-instructions)

---

## Getting Started

### Quick Start

1. **Install Dependencies (Linux)**:
   ```bash
   make install-deps          # Install basic dependencies
   make install-editor-deps   # Install editor dependencies (includes ImGui)
   ```

2. **Try the Editor**:
   ```bash
   make run-editor
   ```

3. **Build Your Game**:
   ```bash
   make linux    # Build for Linux
   make vita      # Build for PS Vita
   ```

### Architecture

The game engine follows a modern architecture with:
- **Scene Graph System**: Hierarchical scene organization with transform inheritance
- **Component System**: Modular, composable entity components
- **Cross-Platform Support**: Shared codebase for PS Vita and Linux
- **Editor Interface**: Visual scene editing tools (Linux only)

---

## Build System

### Build Targets

```bash
# Build for PS Vita
make vita

# Build game for Linux
make linux

# Build editor for Linux
make editor

# Run Linux game
make run

# Run editor
make run-editor

# Debug builds
make debug-linux
make debug-editor

# Clean all builds
make clean
```

### Build Menu in Editor

The editor includes a **Build** menu with two options:

- **Build for Linux**: Creates a Linux executable using OpenGL 2.0 compatible GLSL shaders
- **Build VPK for PS Vita**: Creates a Vita Package (VPK) using CG/HLSL shaders compatible with VitaGL

### Shader Formats

- **Linux Shaders** (`assets/linux_shaders/`): Standard GLSL (#version 120), OpenGL 2.0+
- **Vita Shaders** (`assets/shaders/`): CG (Cg) shaders for VitaGL

---

## Scripting Guide

### Overview

The scripting system allows you to:
- Write game logic in Lua instead of C++
- Hot-reload scripts during development
- Access engine systems (Transform, Input, Physics, Renderer) from Lua
- Create reusable script components

### ScriptComponent

The `ScriptComponent` integrates with the component system and maps Lua script lifecycle to component lifecycle:
- `start()` → Called when component is first added
- `update(deltaTime)` → Called every frame
- `render()` → Called during rendering
- `destroy()` → Called before component is destroyed

### Basic Example

```lua
-- player_controller.lua
function start()
    print("Player controller started")
    local transform = getTransform()
    transform.setPosition(0, 0, 0)
end

function update(deltaTime)
    if input.isActionHeld("MoveForward") then
        local transform = getTransform()
        local pos = transform.getPosition()
        transform.setPosition(pos.x, pos.y, pos.z + deltaTime * 5.0)
    end
end
```

### Lua API

The engine provides bindings for:
- **Transform**: Position, rotation, scale manipulation
- **Input**: Action-based input checking
- **Physics**: Apply forces, check collisions
- **Renderer**: Camera control
- **Scene**: Node creation and management

---

## Input System

### Action-Based Input

Instead of hardcoding keys in Lua scripts, use named actions:

```lua
-- Old way (hardcoded)
if input.isKeyDown("W") then
    -- move forward
end

-- New way (configurable)
if input.isActionHeld("MoveForward") then
    -- move forward
end
```

### Input Mapping File

Input mappings are stored in `input_mappings.txt`:

```
action:MoveForward,PC,0,87,1,0.0,1.0,0
action:MoveBackward,PC,0,83,1,0.0,1.0,0
action:Jump,PC,0,32,0,0.0,1.0,0
```

### Editor Integration

The editor provides a full UI for configuring input mappings:
- Add/remove actions
- Assign keyboard keys, mouse buttons, or analog sticks
- Test mappings in real-time
- Save/load configurations

### Input Types

- `KEYBOARD_KEY`: GLFW key codes (W, A, S, D, Space, etc.)
- `VITA_BUTTON`: Vita controller buttons (Cross, Circle, Square, Triangle, etc.)
- `ANALOG_STICK`: Left/Right analog sticks
- `MOUSE_BUTTON`: Mouse buttons (left, right, middle)
- `MOUSE_AXIS`: Mouse movement (X, Y axes)

---

## Menu System

### Overview

The menu system provides a flexible way to create and manage game menus (main menu, pause menu, settings, etc.) that can be controlled entirely from Lua scripts.

### Features

- **Game pausing**: Menus can pause the game by setting time scale to 0
- **Input handling**: Automatic navigation and selection
- **Screen space rendering**: Menus render on top of the game scene
- **Lua integration**: Full control from Lua scripts

### Menu Types

- `MAIN_MENU` (0): Main menu (pauses game by default)
- `PAUSE_MENU` (1): Pause menu (pauses game by default)
- `SETTINGS_MENU` (2): Settings menu
- `CUSTOM` (3): Custom menu type

### Lua API

```lua
-- Create a menu
MenuManager.createMenu("main_menu", MenuType.MAIN_MENU)

-- Add menu items
MenuManager.addMenuItem("main_menu", "Start Game", function()
    print("Starting game...")
end)

-- Show/hide menus
MenuManager.showMenu("main_menu")
MenuManager.hideMenu("main_menu")
```

---

## Components

### CameraComponent

Provides camera functionality with perspective and orthographic projection modes.

**Key Features:**
- Perspective and orthographic projections
- Configurable FOV, aspect ratio, near/far planes
- Camera controls (keyboard/mouse on Linux, joystick on Vita)
- Active camera management

### MeshRenderer

Renders 3D meshes with materials.

**Key Features:**
- Support for primitive shapes (cube, sphere, plane, etc.)
- Material assignment
- Shadow casting/receiving
- Editor integration

### ModelRenderer

Loads and renders GLTF/GLB 3D models.

**Key Features:**
- GLTF/GLB support
- Automatic texture loading
- Binary model format for Vita optimization
- Model browser in editor

See [docs/COMPONENTS.md](COMPONENTS.md) for detailed component documentation.

### TextComponent

Renders text in 3D world space or 2D screen space.

**Key Features:**
- TTF font support
- Multi-line text with alignment
- World space and screen space rendering modes
- Color and scale customization

### PhysicsComponent

Integrates Bullet Physics for collision detection and physics simulation.

**Key Features:**
- Multiple collision shapes (box, sphere, capsule, cylinder, plane)
- Body types (static, kinematic, dynamic)
- Force and impulse application
- Collision detection callbacks

### ScriptComponent

Attaches Lua scripts to scene nodes.

**Key Features:**
- Hot-reload support
- Lifecycle callbacks (start, update, render, destroy)
- Engine system bindings
- Property access from Lua

### LightComponent

Provides lighting for the scene.

**Key Features:**
- Light types (directional, point, spot)
- Color and intensity control
- Range and attenuation for point/spot lights
- Spotlight angle controls

### Area3DComponent

Non-physical collision detection for trigger zones.

**Key Features:**
- Multiple shape types
- Group management
- Enter/exit/stay callbacks
- Lua integration

### PickupZoneComponent

Specialized component for pickup zones and collectibles.

**Key Features:**
- Non-physical collision detection
- Multiple shape types
- Detection tags
- Lua callbacks

---

## Editor Guide

### Layout

- **Scene Graph (Left)**: Hierarchical view of scene objects
- **Viewport (Center)**: Real-time scene preview and manipulation
- **Properties (Right)**: Object transform and component properties

### Editor Controls

1. **Scene Graph Panel**:
   - Click nodes to select them
   - Right-click for context menu (create child, delete, etc.)
   - Hierarchical scene organization

2. **Properties Panel**:
   - Edit transform properties (position, rotation, scale)
   - Component-specific properties
   - Real-time value updates

3. **Menu Bar**:
   - File operations (New Scene, Open, Save)
   - View toggles for panels
   - Object creation tools
   - Build menu for generating game code

### Creating Objects

Use `Create` menu to add new objects:
- Empty Node: Basic transform node
- Camera: Camera object with CameraComponent
- Cube: Mesh object with MeshRenderer
- Lights: Directional, Point, and Spot lights
- Text: Text rendering component
- Model: GLTF model renderer

### Creating Scenes with Lua Scripts

1. Create objects in the scene
2. Add ScriptComponent to objects
3. Assign Lua script files
4. Configure script properties
5. Save scene
6. Scripts will be loaded when scene is loaded in game

---

## Setup Instructions

### Dependencies Setup

#### Lua (Required for Vita builds)
```bash
git submodule update --init --recursive vendor/lua
./setup_lua.sh
```

#### ImGui (Required for editor)
```bash
./setup_imgui.sh /path/to/imgui/source
```

#### Other Dependencies
- **JSON**: nlohmann/json (header-only, already included)
- **STB**: stb libraries (header-only, already included)
- **Bullet Physics**: Already included with Vita builds
- **TinyGLTF**: Already included

**Note**: JSON, STB, and Lua are managed as Git submodules. When cloning the repository:
```bash
git clone --recursive <repository-url>
```

Or if already cloned:
```bash
git submodule update --init --recursive
```

### Manual Setup

If the setup scripts don't work, you can manually set up dependencies:

#### ImGui Setup
Copy these files from your ImGui download to `vendor/imgui/`:
- Core files: `imgui.h`, `imgui.cpp`, `imgui_demo.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imconfig.h`
- Backend files: `backends/imgui_impl_glfw.h`, `backends/imgui_impl_glfw.cpp`, `backends/imgui_impl_opengl3.h`, `backends/imgui_impl_opengl3.cpp`

#### Lua Setup
1. Initialize submodule: `git submodule update --init --recursive vendor/lua`
2. Build Lua for Vita using the Makefile in `vendor/lua/`

---

## Additional Resources

- Component API documentation: See individual component docs in `docs/` folder
- Example scripts: See `scripts/` folder for Lua script examples
- Scene files: See `assets/scenes/` for example scene configurations

