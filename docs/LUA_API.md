# Lua API Reference

Every binding the engine exposes to game scripts, grouped by table. Names and
signatures are taken from the bindings in
`game_engine/src/Components/ScriptComponent.cpp`; if you add a binding there, add
it here too.

Nodes are always addressed **by name** (the name shown in the editor's scene
graph), and vectors come back as separate return values rather than tables:

```lua
local x, y, z = getNodePosition("Player")
```

**Platform column** — `all` works everywhere. `PC` is compiled out on Vita, `Vita`
is compiled out on desktop. Guard anything platform-specific:

```lua
if input.getMousePosition then ... end     -- nil on Vita
if _VITA_BUILD then ... end                -- global boolean, set on both
```

---

## Contents

- [Script lifecycle](#script-lifecycle)
- [Pointer, rays and picking](#pointer-rays-and-picking)
- [`input`](#input)
- [`ui`](#ui)
- [`Physics`](#physics)
- [`scene`](#scene)
- [`renderer`](#renderer)
- [`camera`](#camera)
- [`animation`](#animation), [`sound`](#sound), [`saveFile`](#savefile), [`Nav`](#nav), [`area3D`](#area3d)
- [Global functions](#global-functions)

---

## Script lifecycle

A script is attached to a node with a ScriptComponent. The engine calls these
free functions if the script defines them:

```lua
function start()             end   -- once, when the scene starts
function update(deltaTime)   end   -- every frame
```

Scripts share modules with `dofile`, which is resolved through the asset path
resolver, so a project-relative path works from any working directory and on the
Vita:

```lua
local levels = dofile("assets/scripts/tire_game/levels.lua")
```

---

## Pointer, rays and picking

One pointer API covers **mouse on desktop and the front touch panel on Vita**, so
a single script drives both. Positions are in framebuffer pixels with `(0,0)` at
the top-left.

A touch has no hover state, so on Vita `isPointerActive()` is only true *while the
panel is being touched*; on desktop it is always true.

### Choosing the right call

| You want to click… | Use | Needs a collider? |
|---|---|---|
| Screen-space UI / menu text | `ui.hitTest(x, y)` | No |
| Any node, by its bounds | `ui.pickNode(x, y)` | No |
| A collider, and you want the surface normal | `Physics.raycastScreenPoint(x, y)` | Yes |
| The raw ray, to do your own maths | `input.getPointerRay()` | — |

**Screen-space text cannot be hit by a world ray** — it has no world position. That
is what `ui.hitTest` is for, and it is the call a mouse- or touch-driven menu
should use. All three helpers default to the current pointer position when you
omit `x, y`.

Nodes that are hidden (`setNodeVisible(name, false)`) or inactive are never
picked, and neither are their children — picking prunes the same subtrees the
renderer does. Hiding something is enough to stop it swallowing clicks.

### Worked example — a clickable menu

From `assets/scripts/main_menu.lua`. Works with a mouse and with Vita touch, with
no scene changes, because the entries are already named screen-space text nodes:

```lua
local function updatePointerSelection()
    if not (ui and ui.hitTest and input and input.isPointerActive) then return end
    if not input.isPointerActive() then return end          -- Vita: only while touching

    local hovered = entryIndexForNode(ui.hitTest())         -- node name under the pointer
    if not hovered then return end

    if hovered ~= selectedIndex then                        -- hover moves the selector
        selectedIndex = hovered
        updateSelectorPosition()
    end

    if input.isPointerPressed() then                        -- press activates it
        handleMenuSelection()
    end
end
```

---

## `input`

### Actions (preferred — remappable via `config/input_mappings.txt`)

| Function | Returns | Platform |
|---|---|---|
| `input.isActionPressed(name)` | bool — first frame only | all |
| `input.isActionHeld(name)` | bool — while down | all |
| `input.isActionReleased(name)` | bool — first frame released | all |
| `input.getActionAxis(name)` | number | all |
| `input.getActionVector2(name)` | x, y | all |

### Pointer (mouse + Vita touch)

| Function | Returns | Platform |
|---|---|---|
| `input.isPointerActive()` | bool — desktop always, Vita while touching | all |
| `input.getPointerPosition()` | x, y in pixels | all |
| `input.isPointerPressed()` | bool — first frame of press/touch | all |
| `input.isPointerHeld()` | bool | all |
| `input.isPointerReleased()` | bool — first frame released | all |
| `input.getPointerRay()` | ox, oy, oz, dx, dy, dz — or `nil` without a camera | all |

### Keys, buttons and sticks

| Function | Returns | Platform |
|---|---|---|
| `input.isKeyPressed(key)` / `input.isKeyDown(key)` | bool | all |
| `input.isButtonPressed(button)` | bool | Vita |
| `input.getLeftStick()` / `input.getRightStick()` | x, y | Vita |
| `input.getMousePosition()` / `input.getMouseDelta()` | x, y | PC |
| `input.isMouseButtonPressed/Held/Released(button)` | bool | PC |
| `input.getMouseWheel()` | number | PC |
| `input.setMouseCapture(enabled)` / `input.isMouseCaptured()` | — / bool | PC |
| `input.setDebugMouseInput(enabled)` | — | PC |

---

## `ui`

| Function | Returns |
|---|---|
| `ui.hitTest([x, y])` | node name under the point, or `nil`. 2D test against laid-out screen-space text |
| `ui.pickNode([x, y])` | node name, distance or `nil`. Nearest node by mesh bounds |

`ui.pickNode` needs no collider: renderable nodes use their mesh bounds, and
lights, cameras and empties get a small box so they stay clickable.

---

## `Physics`

| Function | Returns |
|---|---|
| `Physics.raycast(ox, oy, oz, dx, dy, dz, maxDistance)` | hit table or `nil` |
| `Physics.raycastScreenPoint([x, y, maxDistance])` | hit table or `nil` |

The hit table:

```lua
{ nodeName = "Ground",
  hitDistance = 12.5,
  hitPoint  = { x = 0, y = 0, z = 0 },
  hitNormal = { x = 0, y = 1, z = 0 } }
```

---

## `scene`

| Function | Returns |
|---|---|
| `scene.getName()` | string |
| `scene.loadScene(name)` | bool |
| `scene.loadSceneFromFile(name, path)` | bool |
| `scene.preloadSceneFromFile(name, path)` | bool loads in the background, no switch |
| `scene.createNode(name)` / `scene.destroyNode(name)` | bool |
| `scene.addMeshToNode(...)` / `scene.addPhysicsToNode(...)` | bool |
| `scene.setNodeMaterialOverride(name, table)` | bool a key present enables that override |
| `scene.clearNodeMaterialOverride(name)` | bool |
| `scene.setNodeBlendMode(name, mode)` | bool |

---

## `renderer`

| Function |
|---|
| `renderer.setText(nodeName, text)` |
| `renderer.setTextRenderMode(nodeName, mode)`  `0` world space, `1` screen space |
| `renderer.drawText(...)` |

---

## `camera`

| Function | Returns |
|---|---|
| `camera.getPosition()` / `camera.getRotation()` | x, y, z |
| `camera.setPosition(x, y, z)` / `camera.setRotation(x, y, z)` | — |
| `camera.move(x, y, z)` | — |
| `camera.setNodeLookAt(nodeName)` | — |

---

## `animation`

`animation.addComponent(node)`, `setSkeleton`, `setAnimationClip`, `play`, `pause`,
`stop`, `setLoop`, `setSpeed`, `isPlaying`, `getAvailableSkeletons`,
`getAnimationClips`.

## `sound`

`sound.getComponent(node)`, `sound.play(node)`, `sound.pause(node)`, `sound.stop(node)`.

## `saveFile`

`saveFile.save(path)` and `saveFile.load(path)` — JSON save data in a writable
location, not the read-only asset tree.

## `Nav`

`Nav.find_path(...)`, `Nav.get_agent(...)`, `Nav.world_to_cell(...)`.

## `area3D`

`area3D.getComponent(node)`, `area3D.getComponentsInGroup(group)`.

---

## Global functions

Not namespaced, callable directly.

### Node transform

`getNodePosition(name)` · `setNodePosition(name, x, y, z)` ·
`getNodeLocalPosition` / `setNodeLocalPosition` · `getNodeWorldPosition` ·
`getNodeRotation` / `setNodeRotation` · `setNodeWorldRotation` ·
`getNodeWorldEuler` · `setNodeScale` · `getNodeForward` / `getNodeRight` /
`getNodeUp` · `setNodeLookAt` / `setNodeLookAtPosition` ·
`applyNodeRotationAroundAxis` · `getParent` / `getNodeParentName`

### Node state

`setNodeVisible(name, visible)` · `isNodeVisible(name)` ·
`setNodeActive(name, active)` · `isNodeActive(name)` · `destroyNode(name)`

> `setNodeVisible(name, false)` also stops the node being picked or hit-tested,
> including its children.

### Physics

`physicsRaycast` · `physicsRaycastGround` · `physicsRaycastObstacle` ·
`raycastFromNode` · `raycastFromToWorld` · `getRaycastWorldRay` ·
`setNodeVelocity` / `getNodeVelocity` · `setNodeAngularVelocity` ·
`setNodeBodyType` · `setNodeGravityEnabled` · `setNodeAngularFactor` /
`getNodeAngularFactor` · `getNodePhysicsFriction` ·
`syncNodeTransformToPhysics` · `setJointEnabled` ·
`setPhysicsDebugDrawEnabled` / `getPhysicsDebugDrawEnabled`

### Camera, skybox, misc

`getCameraPosition(name)` · `getActiveCameraPosition()` · `setCameraActive` ·
`getCameraViewport` / `setCameraViewport` · `setActiveSkybox` /
`getActiveSkybox` · `setSkyboxEnabled` · `setSkyboxTextures` ·
`setBeamEndpoints` · `setBeamWidth`

### Engine

| Function | Returns |
|---|---|
| `print(message)` | routed to the console (`sceClibPrintf` on Vita) |
| `getTime()` / `getDeltaTime()` / `getFPS()` | number |
| `isGamePaused()` / `setGamePaused(paused)` | bool / |
| `quitGame()` | stops the loop; exits the process on Vita |
| `dofile(path)` | module — asset-path resolved |
| `_VITA_BUILD` | boolean global, not a function |

### Cross-script calls

`callNodeScriptFunction(node, fn)` and `callNodeScriptFunctionWithParam(node, fn, value)`.
