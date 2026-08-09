TITLEID     := VSDK00420
# Fallback build name, used only when config/build_settings.txt is missing or has
# no name for the platform. The real names live there (pc:executableName,
# vita:vpkName) and the editor writes them.
TARGET      := Baltrogue
EDITOR_TARGET := game_editor

# Source directories
GAME_SOURCES := src
ENGINE_SOURCES := game_engine/src
ENGINE_INCLUDES := game_engine/include
VENDOR_SOURCES := vendor
INCLUDES := include

# Bullet Physics paths
BULLET_INCLUDE := vendor/bullet/src
BULLET_LINUX_LIBS := -Lvendor/bullet/lib -lBulletDynamics -lBulletCollision -lLinearMath
BULLET_VITA_LIBS := -Lvendor/bullet/lib_vita/lib -lBulletDynamics -lBulletCollision -lLinearMath

# OpenAL Soft paths
OPENAL_INCLUDE := vendor/openal-soft/install_linux/include
OPENAL_LINUX_LIBS := -Lvendor/openal-soft/install_linux/lib -Wl,-rpath,$(shell pwd)/vendor/openal-soft/install_linux/lib -lopenal

# Vulkan paths
VULKAN_INCLUDE_FLAGS := -I$(VULKAN_SDK)/include $(shell pkg-config --cflags vulkan 2>/dev/null)
LINUX_VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)

# Allow opting out of Vulkan at make time: `make linux USE_VULKAN=0`
USE_VULKAN ?= 1

ifeq ($(USE_VULKAN),1)
ifeq ($(strip $(LINUX_VULKAN_LIBS)),)
	$(error Vulkan not found. Install: sudo apt install libvulkan-dev vulkan-validationlayers)
endif
endif

# Vita-specific libraries
VITA_LIBS = -lvitaGL -lSceLibKernel_stub -lSceAppMgr_stub -lSceAppUtil_stub -lSceIofilemgr_stub -lmathneon \
    -lc -lSceCommonDialog_stub -lm -lSceGxm_stub -lSceDisplay_stub -lSceSysmodule_stub \
    -lvitashark -lSceShaccCg_stub -lSceKernelDmacMgr_stub -lstdc++ -lSceCtrl_stub \
    -lSceAudio_stub -ltoloader -lSceShaccCgExt -ltaihen_stub -lm -L$(VENDOR_SOURCES)/lua/lib -llua -lz

# Linux-specific libraries (LINUX_VULKAN_LIBS set below when Vulkan sources exist)
ifeq ($(USE_VULKAN),1)
LINUX_LIBS = -lGL -lGLU -lglfw -lGLEW -lm -lpng -lz -lstdc++ -llua5.3 $(OPENAL_LINUX_LIBS) $(LINUX_VULKAN_LIBS)
else
LINUX_LIBS = -lGL -lGLU -lglfw -lGLEW -lm -lpng -lz -lstdc++ -llua5.3 $(OPENAL_LINUX_LIBS)
endif

# Vulkan shader compilation
GLSLC := $(shell command -v glslc 2>/dev/null)

ifeq ($(strip $(GLSLC)),)
	ifneq ($(strip $(VULKAN_SDK)),)
		GLSLC := $(VULKAN_SDK)/bin/glslc
	endif
endif

ifeq ($(strip $(GLSLC)),)
ifeq ($(USE_VULKAN),1)
	$(error glslc not found. Install Vulkan SDK or ensure PATH is set)
endif
endif

# Find all Vulkan shaders automatically
VULKAN_SHADER_DIR := assets/vulkan

VULKAN_SHADERS := $(shell find $(VULKAN_SHADER_DIR) -type f \( \
	-name "*.vert" -o \
	-name "*.frag" -o \
	-name "*.comp" -o \
	-name "*.geom" -o \
	-name "*.tesc" -o \
	-name "*.tese" \
\))

VULKAN_SHADER_SPVS := $(addsuffix .spv,$(VULKAN_SHADERS))

# Linux Editor libraries (no external ImGui needed, we compile our own)
LINUX_EDITOR_LIBS = $(LINUX_LIBS)

# Build directories
BUILD_DIR := build
ifeq ($(USE_VULKAN),1)
LINUX_BUILD_DIR := build_linux
else
LINUX_BUILD_DIR := build_linux_gl
endif
EDITOR_BUILD_DIR := build_editor

# Build names and Vita LiveArea assets.
BUILD_SETTINGS := $(wildcard config/build_settings.txt)
LIVEAREA_SCRIPT := scripts/build_livearea.sh
LIVEAREA_MANIFEST := $(BUILD_DIR)/livearea_files.txt

build_setting = $(if $(BUILD_SETTINGS),$(shell sed -n 's/^$(1)=//p' $(BUILD_SETTINGS) | tail -n 1))

LINUX_TARGET := $(or $(call build_setting,pc:executableName),$(TARGET))
LINUX_GAME := $(LINUX_BUILD_DIR)/$(LINUX_TARGET)

VITA_TITLE := $(or $(call build_setting,vita:title),$(TARGET))
VITA_TITLEID := $(or $(call build_setting,vita:titleId),$(TITLEID))
VITA_APP_VERSION := $(or $(call build_setting,vita:appVersion),01.00)
VITA_VPK := $(BUILD_DIR)/$(or $(call build_setting,vita:vpkName),$(TARGET)).vpk

LIVEAREA_SOURCES := $(wildcard $(foreach key,icon0 pic0 bg0 startup,$(call build_setting,vita:$(key))))

# Game source files
GAME_CFILES := $(foreach dir,$(GAME_SOURCES), $(wildcard $(dir)/*.c))
GAME_CPPFILES := $(foreach dir,$(GAME_SOURCES), $(wildcard $(dir)/*.cpp))

# Engine source files (exclude App - entry points and platform are built separately per target)
ENGINE_CFILES := $(filter-out game_engine/src/App/%, $(wildcard game_engine/src/*/*.c))
ENGINE_CPPFILES := $(filter-out game_engine/src/App/%, $(wildcard game_engine/src/*/*.cpp))

# Vulkan backend (only compiled into Linux game build when enabled)
ifeq ($(USE_VULKAN),1)
VULKAN_CPPFILES := $(wildcard game_engine/src/Rendering/Vulkan/*.cpp)
else
VULKAN_CPPFILES :=
endif

# Vendor source files (ImGui for editor builds)
IMGUI_SOURCES := vendor/imgui
IMGUI_CPPFILES := $(wildcard $(IMGUI_SOURCES)/*.cpp) $(wildcard $(IMGUI_SOURCES)/backends/*.cpp)

# ImGuizmo source files (for editor builds)
IMGUIZMO_SOURCES := vendor/imguizmo
IMGUIZMO_CPPFILES := $(IMGUIZMO_SOURCES)/ImGuizmo.cpp $(IMGUIZMO_SOURCES)/ImSequencer.cpp $(IMGUIZMO_SOURCES)/ImGradient.cpp $(IMGUIZMO_SOURCES)/ImCurveEdit.cpp $(IMGUIZMO_SOURCES)/GraphEditor.cpp

# TinyGLTF source files
TINYGLTF_SOURCES := vendor/tinygltf
TINYGLTF_CPPFILES := $(wildcard $(TINYGLTF_SOURCES)/*.cc)

# All source files for game (Vita main + engine + platform, excluding old game files)
# Include SceneSerializer.cpp for JSON scene loading in Vita builds
# Include pthread stub for Vita (provides pthread compatibility for libstdc++)
ALL_CFILES := $(ENGINE_CFILES) game_engine/src/App/pthread_stub.c
ALL_CPPFILES := game_engine/src/App/vita_main.cpp game_engine/src/App/Platform.cpp $(filter-out game_engine/src/Editor/%, $(ENGINE_CPPFILES)) game_engine/src/Editor/SceneSerializer.cpp game_engine/src/Scene/SceneBinaryFormat.cpp

# Linux game source files (new game main + engine + platform, excluding editor and old game files)
# Include SceneSerializer.cpp for JSON scene loading in game builds, and
# BuildSettings.cpp so the game can read its window title at startup
LINUX_GAME_CFILES := $(ENGINE_CFILES)
LINUX_GAME_CPPFILES := game_engine/src/App/game_main.cpp game_engine/src/App/Platform.cpp $(filter-out game_engine/src/Editor/%, $(ENGINE_CPPFILES)) game_engine/src/Editor/SceneSerializer.cpp game_engine/src/Editor/BuildSettings.cpp game_engine/src/Scene/SceneBinaryFormat.cpp $(VULKAN_CPPFILES)

# Editor source files
EDITOR_SOURCES := game_engine/src/Editor
EDITOR_SOURCE_CPPFILES := $(wildcard $(EDITOR_SOURCES)/*.cpp)

# Editor source files (engine + platform + editor main, excluding game logic files)
EDITOR_PLATFORM_CPPFILES := game_engine/src/App/Platform.cpp
EDITOR_ALL_CPPFILES := $(ENGINE_CPPFILES) $(EDITOR_PLATFORM_CPPFILES) game_engine/src/App/editor_main.cpp

# Object files for Vita build
OBJS := $(addprefix $(BUILD_DIR)/,$(ALL_CFILES:.c=.o) $(ALL_CPPFILES:.cpp=.o))
TINYGLTF_OBJS := $(addprefix $(BUILD_DIR)/,$(TINYGLTF_CPPFILES:.cc=.o))

# Object files for Linux game build
LINUX_OBJS := $(addprefix $(LINUX_BUILD_DIR)/,$(LINUX_GAME_CFILES:.c=.o) $(LINUX_GAME_CPPFILES:.cpp=.o))
LINUX_TINYGLTF_OBJS := $(addprefix $(LINUX_BUILD_DIR)/,$(TINYGLTF_CPPFILES:.cc=.o))

# Object files for Linux editor build (includes editor sources + ImGui + ImGuizmo, no game files)
EDITOR_ALL_CPPFILES_WITH_VENDOR := $(EDITOR_ALL_CPPFILES) $(IMGUI_CPPFILES) $(IMGUIZMO_CPPFILES)
EDITOR_OBJS := $(addprefix $(EDITOR_BUILD_DIR)/,$(EDITOR_ALL_CPPFILES_WITH_VENDOR:.cpp=.o))
EDITOR_TINYGLTF_OBJS := $(addprefix $(EDITOR_BUILD_DIR)/,$(TINYGLTF_CPPFILES:.cc=.o))

# Binary scene assets (JSON converted at build time; save_file stays JSON)
SCENE_JSON_SOURCES := $(filter-out assets/scenes/save_file.json,$(wildcard assets/scenes/*.json))
SCENE_BINARY_FILES := $(SCENE_JSON_SOURCES:.json=.bscn)
SCENE_TO_BINARY := tools/scene_to_binary

# Compiler settings
PREFIX = arm-vita-eabi
CC = $(PREFIX)-gcc
CXX = $(PREFIX)-g++
CFLAGS = -g -Wl,-q -O2 -ftree-vectorize
CXXFLAGS = $(CFLAGS) -fno-exceptions -std=gnu++11 -fpermissive -DBT_THREADSAFE=1
ASFLAGS = $(CFLAGS)

# Linux compiler flags
LINUX_CC = gcc
LINUX_CXX = g++
LINUX_CFLAGS = -g -O2 -Wall
LINUX_CXXFLAGS = $(LINUX_CFLAGS) -std=c++20 -DBT_THREADSAFE=1

# Include paths
ifeq ($(USE_VULKAN),1)
LINUX_INCLUDES = $(VULKAN_INCLUDE_FLAGS) -I$(INCLUDES) -I$(ENGINE_INCLUDES) -I$(BULLET_INCLUDE) -I$(VENDOR_SOURCES)/stb -I/usr/include/lua5.3 -I$(OPENAL_INCLUDE)
else
LINUX_INCLUDES = -I$(INCLUDES) -I$(ENGINE_INCLUDES) -I$(BULLET_INCLUDE) -I$(VENDOR_SOURCES)/stb -I/usr/include/lua5.3 -I$(OPENAL_INCLUDE)
endif
VITA_INCLUDES = -I$(INCLUDES) -I$(ENGINE_INCLUDES) -I$(BULLET_INCLUDE) -I$(VENDOR_SOURCES)/stb -I$(VENDOR_SOURCES)/lua/include/lua
ALL_INCLUDES = $(LINUX_INCLUDES)
EDITOR_INCLUDES = $(ALL_INCLUDES) -I$(VENDOR_SOURCES)/imgui -I$(VENDOR_SOURCES)/imgui/backends -I$(VENDOR_SOURCES)/imguizmo

# Default target
all: vita

# Build Lua for Vita
lua-vita:
	@echo "Building Lua for PS Vita..."
	@if [ ! -f vendor/lua/lib/liblua.a ]; then \
		./setup_lua.sh; \
	else \
		echo "Lua library already built for Vita"; \
	fi


# Vita build
vita: lua-vita scene-binaries $(VITA_VPK)

# LiveArea assets on their own, without building the game
livearea: $(LIVEAREA_MANIFEST)


# Linux game build
ifeq ($(USE_VULKAN),1)
linux: LINUX_CXXFLAGS += -DENABLE_VULKAN
linux: $(VULKAN_SHADER_SPVS) $(LINUX_GAME)
else
linux: $(LINUX_GAME)
endif

# Linux editor build
editor: $(EDITOR_BUILD_DIR)/$(EDITOR_TARGET)

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(LINUX_BUILD_DIR):
	@mkdir -p $(LINUX_BUILD_DIR)

$(EDITOR_BUILD_DIR):
	@mkdir -p $(EDITOR_BUILD_DIR)

# Vita VPK creation
VPK_ASSETS := \
	assets/shaders/lighting.vert=assets/shaders/lighting.vert \
	assets/shaders/lighting.frag=assets/shaders/lighting.frag \
	assets/shaders/text.vert=assets/shaders/text.vert \
	assets/shaders/text.frag=assets/shaders/text.frag \
	assets/shaders/skybox.vert=assets/shaders/skybox.vert \
	assets/shaders/skybox.frag=assets/shaders/skybox.frag \
	assets/shaders/shadow_depth.vert=assets/shaders/shadow_depth.vert \
	assets/shaders/shadow_depth.frag=assets/shaders/shadow_depth.frag \
	config/default_input_mappings.txt=config/default_input_mappings.txt \
	assets/textures/memes/biden_diff_.png=biden_diff_.png \
	assets/textures/terrain/Grass_01_diff.png=Grass_01_diff.png \
	assets/textures/terrain/stonetiles_002_diff.png=stonetiles_002_diff.png \
	assets/textures/skyboxes/skybox_1/right.jpg=right.jpg \
	assets/textures/skyboxes/skybox_1/left.jpg=left.jpg \
	assets/textures/skyboxes/skybox_1/top.jpg=top.jpg \
	assets/textures/skyboxes/skybox_1/bottom.jpg=bottom.jpg \
	assets/textures/skyboxes/skybox_1/front.jpg=front.jpg \
	assets/textures/skyboxes/skybox_1/back.jpg=back.jpg \
	assets/fonts/DroidSans.ttf=assets/fonts/DroidSans.ttf \
	assets/models/hedge_small.glb=assets/models/hedge_small.glb \
	assets/models/well.glb=assets/models/well.glb \
	assets/models/SignPlate1.glb=assets/models/SignPlate1.glb \
	assets/scenes/main_menu.bscn=assets/scenes/main_menu.bscn \
	assets/scenes/level_1.bscn=assets/scenes/level_1.bscn \
	assets/scenes/level_2.bscn=assets/scenes/level_2.bscn \
	assets/scripts/main_menu.lua=assets/scripts/main_menu.lua \
	assets/scripts/pause_menu.lua=assets/scripts/pause_menu.lua \
	assets/scripts/ui/controls_panel.lua=assets/scripts/ui/controls_panel.lua \
	assets/scripts/ui/level_signs.lua=assets/scripts/ui/level_signs.lua \
	assets/scripts/ui/option_selector.lua=assets/scripts/ui/option_selector.lua \
	assets/scripts/tire_game/config.lua=assets/scripts/tire_game/config.lua \
	assets/scripts/tire_game/levels.lua=assets/scripts/tire_game/levels.lua \
	assets/scripts/tire_game/player_controller.lua=assets/scripts/tire_game/player_controller.lua \
	assets/scripts/tire_game/tire_camera.lua=assets/scripts/tire_game/tire_camera.lua \
	assets/scripts/tire_game/tire_camera_collision.lua=assets/scripts/tire_game/tire_camera_collision.lua \
	assets/scripts/tire_game/tire_checkpoint.lua=assets/scripts/tire_game/tire_checkpoint.lua \
	assets/scripts/tire_game/tire_ground.lua=assets/scripts/tire_game/tire_ground.lua \
	assets/scripts/tire_game/tire_hud.lua=assets/scripts/tire_game/tire_hud.lua \
	assets/scripts/tire_game/tire_input.lua=assets/scripts/tire_game/tire_input.lua \
	assets/scripts/tire_game/tire_level_flow.lua=assets/scripts/tire_game/tire_level_flow.lua \
	assets/scripts/tire_game/tire_movement.lua=assets/scripts/tire_game/tire_movement.lua \
	assets/scripts/tire_game/tire_physics.lua=assets/scripts/tire_game/tire_physics.lua \
	assets/scripts/tire_game/tire_save.lua=assets/scripts/tire_game/tire_save.lua \
	assets/scripts/tire_game/tire_spawn.lua=assets/scripts/tire_game/tire_spawn.lua

VPK_OPTIONAL_ASSETS := textures.txt fonts.txt scripts.txt config/input_mappings.txt config/shadow_settings.txt
VPK_OPTIONAL_PRESENT := $(wildcard $(VPK_OPTIONAL_ASSETS))

VPK_ASSET_SOURCES := $(foreach pair,$(VPK_ASSETS),$(firstword $(subst =, ,$(pair))))

$(LIVEAREA_MANIFEST): $(BUILD_SETTINGS) $(LIVEAREA_SOURCES) $(LIVEAREA_SCRIPT) | $(BUILD_DIR)
	$(LIVEAREA_SCRIPT) $@

$(VITA_VPK): $(BUILD_DIR)/eboot.bin $(LIVEAREA_MANIFEST) $(VPK_ASSET_SOURCES) $(VPK_OPTIONAL_PRESENT)
	vita-mksfoex -s TITLE_ID=$(VITA_TITLEID) -s APP_VER=$(VITA_APP_VERSION) "$(VITA_TITLE)" $(BUILD_DIR)/param.sfo
	vita-pack-vpk -s $(BUILD_DIR)/param.sfo -b $(BUILD_DIR)/eboot.bin \
		$(addprefix -a ,$(VPK_ASSETS)) \
		$(foreach f,$(VPK_OPTIONAL_PRESENT),-a $(f)=$(f)) \
		$$(sed 's/^/-a /' $(LIVEAREA_MANIFEST)) \
		$@

$(BUILD_DIR)/eboot.bin: $(BUILD_DIR)/$(TARGET).velf
	vita-make-fself -s $< $@

$(BUILD_DIR)/%.velf: $(BUILD_DIR)/%.elf
	vita-elf-create -s $< $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS) $(TINYGLTF_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ $(BULLET_VITA_LIBS) $(VITA_LIBS) -o $@


# Linux game executable
$(LINUX_GAME): $(LINUX_OBJS) $(LINUX_TINYGLTF_OBJS) | $(LINUX_BUILD_DIR)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $^ $(LINUX_LIBS) $(BULLET_LINUX_LIBS) -o $@

%.vert.spv: %.vert
	@echo "[GLSLC] $<"
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

%.frag.spv: %.frag
	@echo "[GLSLC] $<"
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

%.comp.spv: %.comp
	@echo "[GLSLC] $<"
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

%.geom.spv: %.geom
	@echo "[GLSLC] $<"
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

%.tesc.spv: %.tesc
	@echo "[GLSLC] $<"
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

%.tese.spv: %.tese
	@echo "[GLSLC] $<"
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

# Linux editor executable
$(EDITOR_BUILD_DIR)/$(EDITOR_TARGET): $(EDITOR_OBJS) $(EDITOR_TINYGLTF_OBJS) | $(EDITOR_BUILD_DIR)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $^ $(LINUX_EDITOR_LIBS) $(BULLET_LINUX_LIBS) -o $@

# Convert JSON scenes to binary MessagePack format
scene-binaries: $(SCENE_BINARY_FILES)

$(SCENE_TO_BINARY): tools/scene_to_binary.cpp game_engine/src/Scene/SceneBinaryFormat.cpp
	@mkdir -p $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $(LINUX_INCLUDES) -I$(ENGINE_INCLUDES) $^ -o $@

assets/scenes/%.bscn: assets/scenes/%.json $(SCENE_TO_BINARY)
	$(SCENE_TO_BINARY) $< $@

# Build rules for C files (Vita)
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VITA_INCLUDES) -DVITA_BUILD -DLUA_USE_C89 -c $< -o $@

# Build rules for C++ files (Vita)
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(VITA_INCLUDES) -DVITA_BUILD -DLUA_USE_C89 -c $< -o $@

# Build rules for .cc files (Vita)
$(BUILD_DIR)/%.o: %.cc | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(VITA_INCLUDES) -DVITA_BUILD -DLUA_USE_C89 -c $< -o $@

# Build rules for C files (Linux Game)
$(LINUX_BUILD_DIR)/%.o: %.c | $(LINUX_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(LINUX_CC) $(LINUX_CFLAGS) $(ALL_INCLUDES) -DLINUX_BUILD -c $< -o $@

# Build rules for C++ files (Linux Game)
$(LINUX_BUILD_DIR)/%.o: %.cpp | $(LINUX_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $(ALL_INCLUDES) -DLINUX_BUILD -c $< -o $@

# Build rules for .cc files (Linux Game)
$(LINUX_BUILD_DIR)/%.o: %.cc | $(LINUX_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $(ALL_INCLUDES) -DLINUX_BUILD -c $< -o $@

# Build rules for C files (Linux Editor)
$(EDITOR_BUILD_DIR)/%.o: %.c | $(EDITOR_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(LINUX_CC) $(LINUX_CFLAGS) $(EDITOR_INCLUDES) -DLINUX_BUILD -DEDITOR_BUILD -c $< -o $@

# Build rules for C++ files (Linux Editor)
$(EDITOR_BUILD_DIR)/%.o: %.cpp | $(EDITOR_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $(EDITOR_INCLUDES) -DLINUX_BUILD -DEDITOR_BUILD -c $< -o $@

# Build rules for .cc files (Linux Editor)
$(EDITOR_BUILD_DIR)/%.o: %.cc | $(EDITOR_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $(EDITOR_INCLUDES) -DLINUX_BUILD -DEDITOR_BUILD -c $< -o $@

# Special build rules for vendor/imguizmo (suppress warnings from third-party code)
$(EDITOR_BUILD_DIR)/vendor/imguizmo/%.o: vendor/imguizmo/%.cpp | $(EDITOR_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable $(EDITOR_INCLUDES) -DLINUX_BUILD -DEDITOR_BUILD -c $< -o $@

# Clean all builds
clean:
	@rm -rf $(BUILD_DIR) build_linux build_linux_gl $(EDITOR_BUILD_DIR)
	@find $(VULKAN_SHADER_DIR) -name "*.spv" -delete

clean-vita:
	@rm -rf $(BUILD_DIR)

clean-linux:
	@rm -rf build_linux build_linux_gl
	@find $(VULKAN_SHADER_DIR) -name "*.spv" -delete

clean-editor:
	@rm -rf $(EDITOR_BUILD_DIR)

# Also drop the generated binary scenes (only needed if the converter changed)
clean-scenes:
	@rm -f $(SCENE_BINARY_FILES) $(SCENE_TO_BINARY)

# Clean + rebuild a single target, leaving the other build trees untouched.
# Recursive make instead of prerequisites so the order holds under -j.
rebuild-vita:
	@$(MAKE) clean-vita
	@$(MAKE) vita

rebuild-linux:
	@$(MAKE) clean-linux
	@$(MAKE) linux

rebuild-editor:
	@$(MAKE) clean-editor
	@$(MAKE) editor

# Install Linux dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libglfw3-dev libglew-dev libpng-dev libgl1-mesa-dev liblua5.3-dev libvulkan-dev vulkan-headers ffmpeg pngquant

# Install editor dependencies (ImGui is compiled from vendor/ folder)
install-editor-deps: install-deps
	@echo "Editor dependencies installed. ImGui will be compiled from vendor/ folder."

# Run Linux build
run: linux
	$(LINUX_GAME)

# Run editor build
run-editor: editor
	$(EDITOR_BUILD_DIR)/$(EDITOR_TARGET)

# Debug builds
ifeq ($(USE_VULKAN),1)
debug-linux: LINUX_CXXFLAGS += -DENABLE_VULKAN -DDEBUG -O0
else
debug-linux: LINUX_CXXFLAGS += -DDEBUG -O0
endif
debug-linux: linux

debug-editor: LINUX_CXXFLAGS += -DDEBUG -O0
debug-editor: editor

# Help target
help:
	@echo "Available targets:"
	@echo "  vita           - Build for PS Vita (default)"
	@echo "  livearea       - Build sce_sys/ from config/build_settings.txt only"
	@echo "  linux          - Build game for Linux (includes Rendering/Vulkan/*.cpp)"
	@echo "  editor         - Build editor for Linux"
	@echo "  run            - Run Linux game build"
	@echo "  run-editor     - Run Linux editor build"
	@echo "  debug-linux    - Build Linux game with debug symbols"
	@echo "  debug-editor   - Build Linux editor with debug symbols"
	@echo "  build-bullet   - Build Bullet Physics libraries"
	@echo "  clean          - Clean all build directories"
	@echo "  clean-vita     - Clean only build/ (Vita objects + vpk)"
	@echo "  clean-linux    - Clean only build_linux/, build_linux_gl/ and .spv shaders"
	@echo "  clean-editor   - Clean only build_editor/"
	@echo "  clean-scenes   - Remove generated .bscn files and the converter"
	@echo "  rebuild-vita   - clean-vita then vita (other builds untouched)"
	@echo "  rebuild-linux  - clean-linux then linux"
	@echo "  rebuild-editor - clean-editor then editor"
	@echo "  install-deps   - Install Linux dependencies"
	@echo "  install-editor-deps - Install editor dependencies"
	@echo "  lua-vita       - Build Lua 5.3 static library for PS Vita"
	@echo "  help           - Show this help message"

# Build Bullet Physics libraries
build-bullet:
	@echo "Building Bullet Physics libraries..."
	@if [ ! -d "$(BULLET_INCLUDE)/lib" ]; then mkdir -p $(BULLET_INCLUDE)/lib; fi
	@echo "Note: You need to manually build Bullet Physics libraries for your platforms:"
	@echo "  - Linux: Use CMake or the provided build system in vendor/bullet"
	@echo "  - Vita: Cross-compile using VitaSDK toolchain"
	@echo "  - Place the built .a files in vendor/bullet/lib/"

.PHONY: all vita livearea linux editor run run-editor clean clean-vita clean-linux clean-editor clean-scenes rebuild-vita rebuild-linux rebuild-editor install-deps install-editor-deps debug-linux debug-editor help build-bullet lua-vita scene-binaries