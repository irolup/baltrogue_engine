#!/bin/bash
# Unified setup script for Baltrogue Engine dependencies
# Usage: ./setup.sh [lua|imgui|all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

show_usage() {
    echo "Usage: $0 [lua|imgui|all]"
    echo ""
    echo "Options:"
    echo "  lua    - Setup Lua submodule for Vita builds"
    echo "  imgui  - Setup ImGui for editor (requires path to ImGui source)"
    echo "  all    - Setup all dependencies (requires ImGui path)"
    echo ""
    echo "Examples:"
    echo "  $0 lua"
    echo "  $0 imgui ~/Downloads/imgui-1.90.0"
    echo "  $0 all ~/Downloads/imgui-1.90.0"
}

setup_lua() {
    echo "=== Setting up Lua submodule for Vita ==="
    VENDOR_DIR="vendor/lua"

    if [ ! -d "$VENDOR_DIR/.git" ]; then
        echo "Initializing Lua submodule..."
        git submodule update --init --recursive vendor/lua
        if [ $? -ne 0 ]; then
            echo "Error: Failed to initialize Lua submodule"
            exit 1
        fi
    else
        echo "Lua submodule already initialized"
    fi

    mkdir -p "$VENDOR_DIR/include/lua"
    mkdir -p "$VENDOR_DIR/lib"

    echo "Building Lua for Vita..."
    cd "$VENDOR_DIR"

    cat > Makefile.vita << 'EOF'
CC = arm-vita-eabi-gcc
AR = arm-vita-eabi-ar
CFLAGS = -O2 -Wall -DLUA_COMPAT_5_3 -DLUA_USE_C89
TARGET = liblua.a

CORE_FILES = lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c
LIB_FILES = lauxlib.c lbaselib.c lcorolib.c ldblib.c lmathlib.c lstrlib.c ltablib.c lutf8lib.c linit.c

SRC_FILES = $(CORE_FILES) $(LIB_FILES)
OBJ_FILES = $(SRC_FILES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	cp lua.h luaconf.h lualib.h lauxlib.h include/lua/
	cp $(TARGET) lib/

clean:
	rm -f $(OBJ_FILES) $(TARGET)

.PHONY: all install clean
EOF

    cat > linit_vita.c << 'EOF'
#define linit_c
#define LUA_LIB

#include <stddef.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "llimits.h"

static const luaL_Reg stdlibs[] = {
  {LUA_GNAME, luaopen_base},
  {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_DBLIBNAME, luaopen_debug},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_UTF8LIBNAME, luaopen_utf8},
  {NULL, NULL}
};

LUALIB_API void luaL_openselectedlibs (lua_State *L, int load, int preload) {
  int mask;
  const luaL_Reg *lib;
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {
      luaL_requiref(L, lib->name, lib->func, 1);
      lua_pop(L, 1);
    }
    else if (preload & mask) {
      lua_pushcfunction(L, lib->func);
      lua_setfield(L, -2, lib->name);
    }
  }
  lua_pop(L, 1);
}
EOF

    mv linit.c linit_original.c
    mv linit_vita.c linit.c

    make -f Makefile.vita install

    if [ $? -eq 0 ]; then
        echo ""
        echo "Lua setup complete!"
        echo "Files created:"
        echo "  - $VENDOR_DIR/include/lua/ (headers)"
        echo "  - $VENDOR_DIR/lib/liblua.a (Vita library)"
    else
        echo "Error: Failed to build Lua for Vita"
        exit 1
    fi

    cd "$SCRIPT_DIR"
}

setup_imgui() {
    if [ "$#" -lt 1 ]; then
        echo "Error: ImGui source path required"
        echo "Usage: $0 imgui /path/to/imgui/source"
        exit 1
    fi

    IMGUI_SOURCE="$1"
    VENDOR_DIR="vendor/imgui"

    if [ ! -d "$IMGUI_SOURCE" ]; then
        echo "Error: ImGui source directory '$IMGUI_SOURCE' does not exist"
        exit 1
    fi

    echo "=== Setting up ImGui from: $IMGUI_SOURCE ==="

    echo "Copying core ImGui files..."
    cp "$IMGUI_SOURCE/imgui.h" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imgui.cpp" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imgui_demo.cpp" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imgui_draw.cpp" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imgui_tables.cpp" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imgui_widgets.cpp" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imconfig.h" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imgui_internal.h" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imstb_rectpack.h" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imstb_textedit.h" "$VENDOR_DIR/"
    cp "$IMGUI_SOURCE/imstb_truetype.h" "$VENDOR_DIR/"

    echo "Copying backend files..."
    mkdir -p "$VENDOR_DIR/backends"
    cp "$IMGUI_SOURCE/backends/imgui_impl_glfw.h" "$VENDOR_DIR/backends/"
    cp "$IMGUI_SOURCE/backends/imgui_impl_glfw.cpp" "$VENDOR_DIR/backends/"
    cp "$IMGUI_SOURCE/backends/imgui_impl_opengl3.h" "$VENDOR_DIR/backends/"
    cp "$IMGUI_SOURCE/backends/imgui_impl_opengl3.cpp" "$VENDOR_DIR/backends/"
    cp "$IMGUI_SOURCE/backends/imgui_impl_opengl3_loader.h" "$VENDOR_DIR/backends/"

    echo ""
    echo "ImGui setup complete!"
    echo "You can now build the editor with: make editor"
}

case "$1" in
    lua)
        setup_lua
        ;;
    imgui)
        if [ -z "$2" ]; then
            echo "Error: ImGui source path required"
            show_usage
            exit 1
        fi
        setup_imgui "$2"
        ;;
    all)
        if [ -z "$2" ]; then
            echo "Error: ImGui source path required for 'all' option"
            show_usage
            exit 1
        fi
        setup_lua
        setup_imgui "$2"
        ;;
    *)
        show_usage
        exit 1
        ;;
esac

