TITLEID     := VSDK00420
TARGET      := first_game
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

# Vita-specific libraries
VITA_LIBS = -lvitaGL -lSceLibKernel_stub -lSceAppMgr_stub -lSceAppUtil_stub -lSceIofilemgr_stub -lmathneon \
    -lc -lSceCommonDialog_stub -lm -lSceGxm_stub -lSceDisplay_stub -lSceSysmodule_stub \
    -lvitashark -lSceShaccCg_stub -lSceKernelDmacMgr_stub -lstdc++ -lSceCtrl_stub \
    -lSceAudio_stub -ltoloader -lSceShaccCgExt -ltaihen_stub -lm -L$(VENDOR_SOURCES)/lua/lib -llua -lz

# Linux-specific libraries
LINUX_LIBS = -lGL -lGLU -lglfw -lGLEW -lm -lpng -lz -lstdc++ -llua5.3 $(OPENAL_LINUX_LIBS)

# Linux Editor libraries (no external ImGui needed, we compile our own)
LINUX_EDITOR_LIBS = $(LINUX_LIBS)

# Build directories
BUILD_DIR := build
LINUX_BUILD_DIR := build_linux
EDITOR_BUILD_DIR := build_editor

# Game source files
GAME_CFILES := $(foreach dir,$(GAME_SOURCES), $(wildcard $(dir)/*.c))
GAME_CPPFILES := $(foreach dir,$(GAME_SOURCES), $(wildcard $(dir)/*.cpp))

# Engine source files
ENGINE_CFILES := $(foreach dir,$(ENGINE_SOURCES), $(wildcard $(dir)/*/*.c))
ENGINE_CPPFILES := $(foreach dir,$(ENGINE_SOURCES), $(wildcard $(dir)/*/*.cpp))

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
ALL_CFILES := $(ENGINE_CFILES) src/pthread_stub.c
ALL_CPPFILES := src/vita_main.cpp src/Platform.cpp $(filter-out game_engine/src/Editor/%, $(ENGINE_CPPFILES)) game_engine/src/Editor/SceneSerializer.cpp

# Linux game source files (new game main + engine + platform, excluding editor and old game files)
# Include SceneSerializer.cpp for JSON scene loading in game builds
LINUX_GAME_CFILES := $(ENGINE_CFILES)
LINUX_GAME_CPPFILES := src/game_main.cpp src/Platform.cpp $(filter-out game_engine/src/Editor/%, $(ENGINE_CPPFILES)) game_engine/src/Editor/SceneSerializer.cpp

# Editor source files
EDITOR_SOURCES := game_engine/src/Editor
EDITOR_SOURCE_CPPFILES := $(wildcard $(EDITOR_SOURCES)/*.cpp)

# Editor source files (engine + platform + editor main, excluding game logic files)
EDITOR_PLATFORM_CPPFILES := src/Platform.cpp
EDITOR_ALL_CPPFILES := $(ENGINE_CPPFILES) $(EDITOR_PLATFORM_CPPFILES) editor_main.cpp

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
LINUX_CXXFLAGS = $(LINUX_CFLAGS) -std=c++17 -DBT_THREADSAFE=1

# Include paths
LINUX_INCLUDES = -I$(INCLUDES) -I$(ENGINE_INCLUDES) -I$(BULLET_INCLUDE) -I$(VENDOR_SOURCES)/stb -I/usr/include/lua5.3 -I$(OPENAL_INCLUDE)
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
vita: lua-vita $(BUILD_DIR)/$(TARGET).vpk


# Linux game build
linux: $(LINUX_BUILD_DIR)/$(TARGET)

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
$(BUILD_DIR)/$(TARGET).vpk: $(BUILD_DIR)/eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLEID) "$(TARGET)" $(BUILD_DIR)/param.sfo
	vita-pack-vpk -s $(BUILD_DIR)/param.sfo -b $(BUILD_DIR)/eboot.bin \
		-a assets/shaders/lambertian.vert=assets/shaders/lambertian.vert \
		-a assets/shaders/lambertian.frag=assets/shaders/lambertian.frag \
		-a assets/shaders/lambertian_hlsl.vert=assets/shaders/lambertian_hlsl.vert \
		-a assets/shaders/lambertian_hlsl.frag=assets/shaders/lambertian_hlsl.frag \
		-a assets/shaders/lighting.vert=assets/shaders/lighting.vert \
		-a assets/shaders/lighting.frag=assets/shaders/lighting.frag \
		-a assets/shaders/text.vert=assets/shaders/text.vert \
		-a assets/shaders/text.frag=assets/shaders/text.frag \
		-a assets/shaders/skybox.vert=assets/shaders/skybox.vert \
		-a assets/shaders/skybox.frag=assets/shaders/skybox.frag \
		-a textures.txt=textures.txt \
		-a fonts.txt=fonts.txt \
		-a scripts.txt=scripts.txt \
		-a input_mappings.txt=input_mappings.txt \
		-a assets/textures/checkered_pavement_tiles/checkered_pavement_tiles_arm_1k.png=checkered_pavement_tiles_arm_1k.png \
		-a assets/textures/checkered_pavement_tiles/checkered_pavement_tiles_diff_1k.png=checkered_pavement_tiles_diff_1k.png \
		-a assets/textures/checkered_pavement_tiles/checkered_pavement_tiles_nor_gl_1k.png=checkered_pavement_tiles_nor_gl_1k.png \
		-a assets/textures/memes/asu_diff_.png=asu_diff_.png \
		-a assets/textures/memes/yuri_diff_.png=yuri_diff_.png \
		-a assets/textures/memes/asusad_diff_.png=asusad_diff_.png \
		-a assets/textures/memes/biden_diff_.png=biden_diff_.png \
		-a assets/textures/memes/spider_diff_.png=spider_diff_.png \
		-a assets/textures/memes/saul_diff_.png=saul_diff_.png \
		-a assets/textures/memes/retard_diff_.png=retard_diff_.png \
		-a assets/textures/red_brick/red_brick_diff_1k.png=red_brick_diff_1k.png \
		-a assets/textures/red_brick/red_brick_arm_1k.png=red_brick_arm_1k.png \
		-a assets/textures/red_brick/red_brick_nor_gl_1k.png=red_brick_nor_gl_1k.png \
		-a assets/textures/skyboxes/skybox_1/right.jpg=right.jpg \
		-a assets/textures/skyboxes/skybox_1/left.jpg=left.jpg \
		-a assets/textures/skyboxes/skybox_1/top.jpg=top.jpg \
		-a assets/textures/skyboxes/skybox_1/bottom.jpg=bottom.jpg \
		-a assets/textures/skyboxes/skybox_1/front.jpg=front.jpg \
		-a assets/textures/skyboxes/skybox_1/back.jpg=back.jpg \
		-a assets/fonts/DroidSans.ttf=assets/fonts/DroidSans.ttf \
		-a assets/fonts/ProggyVector.ttf=assets/fonts/ProggyVector.ttf \
		-a assets/fonts/ProggyClean.ttf=assets/fonts/ProggyClean.ttf \
		-a assets/fonts/Ubuntu.ttf=assets/fonts/Ubuntu.ttf \
		-a assets/fonts/sui.ttf=assets/fonts/sui.ttf \
		-a assets/models/Player.glb=assets/models/Player.glb \
		-a assets/models/lightning.glb=assets/models/lightning.glb \
		-a assets/audios/walk_sound.wav=assets/audios/walk_sound.wav \
		-a scripts/pause_menu.lua=scripts/pause_menu.lua \
		-a scripts/collectible_behavior.lua=scripts/collectible_behavior.lua \
		-a scripts/player_controller.lua=scripts/player_controller.lua \
		-a scripts/main_menu.lua=scripts/main_menu.lua \
		-a scripts/goal_controller.lua=scripts/goal_controller.lua \
		-a scripts/animated_character.lua=scripts/animated_character.lua \
		-a assets/scenes/main_menu.json=assets/scenes/main_menu.json \
		-a assets/scenes/first_game_demo.json=assets/scenes/first_game_demo.json \
 $@
$(BUILD_DIR)/eboot.bin: $(BUILD_DIR)/$(TARGET).velf
	vita-make-fself -s $< $@

$(BUILD_DIR)/%.velf: $(BUILD_DIR)/%.elf
	vita-elf-create $< $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS) $(TINYGLTF_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ $(BULLET_VITA_LIBS) $(VITA_LIBS) -o $@


# Text test executable
TEXT_TEST_TARGET := text_test
TEXT_TEST_CPPFILES := src/text_test.cpp src/Platform.cpp $(filter-out game_engine/src/Editor/%, $(ENGINE_CPPFILES))
TEXT_TEST_OBJS := $(addprefix $(LINUX_BUILD_DIR)/,$(LINUX_GAME_CFILES:.c=.o) $(TEXT_TEST_CPPFILES:.cpp=.o))

# Lua test executable
LUA_TEST_TARGET := lua_test
LUA_TEST_CPPFILES := src/lua_test.cpp src/Platform.cpp $(filter-out game_engine/src/Editor/%, $(ENGINE_CPPFILES))
LUA_TEST_OBJS := $(addprefix $(LINUX_BUILD_DIR)/,$(LINUX_GAME_CFILES:.c=.o) $(LUA_TEST_CPPFILES:.cpp=.o))

# Linux game executable
$(LINUX_BUILD_DIR)/$(TARGET): $(LINUX_OBJS) $(LINUX_TINYGLTF_OBJS) | $(LINUX_BUILD_DIR)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $^ $(LINUX_LIBS) $(BULLET_LINUX_LIBS) -o $@

# Linux editor executable
$(EDITOR_BUILD_DIR)/$(EDITOR_TARGET): $(EDITOR_OBJS) $(EDITOR_TINYGLTF_OBJS) | $(EDITOR_BUILD_DIR)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $^ $(LINUX_EDITOR_LIBS) $(BULLET_LINUX_LIBS) -o $@

# Text test executable
$(LINUX_BUILD_DIR)/$(TEXT_TEST_TARGET): $(TEXT_TEST_OBJS) | $(LINUX_BUILD_DIR)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $^ $(LINUX_LIBS) $(BULLET_LINUX_LIBS) -o $@

# Lua test executable
$(LINUX_BUILD_DIR)/$(LUA_TEST_TARGET): $(LUA_TEST_OBJS) $(LINUX_TINYGLTF_OBJS) | $(LINUX_BUILD_DIR)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) $^ $(LINUX_LIBS) $(BULLET_LINUX_LIBS) -o $@

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
	@rm -rf $(BUILD_DIR) $(LINUX_BUILD_DIR) $(EDITOR_BUILD_DIR)

# Install Linux dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libglfw3-dev libglew-dev libpng-dev libgl1-mesa-dev liblua5.3-dev

# Install editor dependencies (ImGui is compiled from vendor/ folder)
install-editor-deps: install-deps
	@echo "Editor dependencies installed. ImGui will be compiled from vendor/ folder."

# Run Linux build
run: linux
	$(LINUX_BUILD_DIR)/$(TARGET)

# Run editor build
run-editor: editor
	$(EDITOR_BUILD_DIR)/$(EDITOR_TARGET)

# Debug builds
debug-linux: LINUX_CXXFLAGS += -DDEBUG -O0
debug-linux: linux

debug-editor: LINUX_CXXFLAGS += -DDEBUG -O0
debug-editor: editor

# Help target
help:
	@echo "Available targets:"
	@echo "  vita           - Build for PS Vita (default)"
	@echo "  linux          - Build game for Linux"
	@echo "  editor         - Build editor for Linux"
	@echo "  run            - Run Linux game build"
	@echo "  run-editor     - Run Linux editor build"
	@echo "  debug-linux    - Build Linux game with debug symbols"
	@echo "  debug-editor   - Build Linux editor with debug symbols"
	@echo "  build-bullet   - Build Bullet Physics libraries"
	@echo "  clean          - Clean all build directories"
	@echo "  install-deps   - Install Linux dependencies"
	@echo "  install-editor-deps - Install editor dependencies"
	@echo "  lua-test       - Build Lua scripting test executable"
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

# Text test target
text-test: $(LINUX_BUILD_DIR)/$(TEXT_TEST_TARGET)

# Lua test target
lua-test: $(LINUX_BUILD_DIR)/$(LUA_TEST_TARGET)

.PHONY: all vita linux editor run run-editor clean install-deps install-editor-deps debug-linux debug-editor help build-bullet text-test lua-test lua-vita