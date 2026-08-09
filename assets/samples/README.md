# Engine samples

Feature showcases for the engine.

Nothing here ships. These scenes are Linux/editor-only:

- they are excluded from `VPK_ASSETS` in the `Makefile`, so no Vita build packs them
- they are outside `assets/scenes/`, so the `scene-binaries` make step does not
  compile them to `.bscn`

Open them from the editor (**File → Open Scene**, browse to `assets/samples/scenes/`)
and run them with the editor build.

## What each sample demonstrates

### `scenes/playground.json`
FPS-style character controller, weapons, enemies with nav-mesh pathing and
line-of-sight, physics interactables.

- `scripts/player_controller.lua`  walk/look/jump controller; lazily `dofile`s the
  weapon modules below
- `scripts/gravity_gun.lua`, `scripts/impulse_pistol.lua`, `scripts/melee.lua`,
  `scripts/gravity_prism.lua`  weapons, raycasts and impulse forces
- `scripts/enemy_main.lua` -> `scripts/enemy_chase.lua` -> `scripts/vision.lua` 
  enemy state, nav-agent chase, line-of-sight checks with an observer exclusion
- `scripts/interactables/piston_button.lua`  physics-triggered button
- `models/Pistol.glb`

### `scenes/first_game_demo.json`
The original demo: third-person player, collectibles, a goal volume, skeletal
animation.

- `scripts/player_controller_demo.lua`  third-person controller
- `scripts/collectible_behavior.lua`, `scripts/goal_controller.lua`  Area3D
  triggers and win condition
- `scripts/animated_character.lua`  GLTF skeletal animation playback
- `models/Player.glb`, `models/lightning.glb`, `models/Pistol.glb`


## Shared assets these samples still use

They reference a few things that live outside this folder on purpose, because a
current game project uses them too, do not move these in:

- `assets/scripts/pause_menu.lua`
- `assets/fonts/`, `assets/shaders/`, `assets/linux_shaders/`
- `assets/textures/` — the texture library is shared and recursively scanned by
  `TextureManager::discoverAllTextures("assets/textures")`, which is what fills the
  editor's texture picker. Splitting it would hide those textures from the editor.
