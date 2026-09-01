#include "Scene/SceneScriptRuntime.h"
#include "Components/ScriptComponent.h"
#include "Core/MenuManager.h"
#include "Core/AssetPaths.h"

#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace GameEngine {

SceneScriptRuntime::SceneScriptRuntime()
    : luaState(nullptr)
    , engineBindingsInstalled(false)
{
}

SceneScriptRuntime::~SceneScriptRuntime() {
    shutdown();
}

bool SceneScriptRuntime::isInitialized() const {
    return luaState != nullptr;
}

lua_State* SceneScriptRuntime::getLuaState() {
    return luaState;
}

bool SceneScriptRuntime::ensureInitialized() {
    if (luaState) {
        return true;
    }

    luaState = luaL_newstate();
    if (!luaState) {
        return false;
    }

    luaL_openlibs(luaState);
    ScriptComponent::installEngineBindings(luaState);
    engineBindingsInstalled = true;
    return true;
}

void SceneScriptRuntime::shutdown() {
    if (!luaState) {
        return;
    }

    // MenuManager may still hold this state (last scene that bound it)
    // clear it before closing so menu callbacks never touch freed memory
    MenuManager::getInstance().onLuaStateClosed(luaState);

    lua_close(luaState);
    luaState = nullptr;
    engineBindingsInstalled = false;
}

void SceneScriptRuntime::setActiveComponent(ScriptComponent* component) {
    if (!luaState) {
        return;
    }

    lua_pushlightuserdata(luaState, component);
    lua_setglobal(luaState, "_currentScriptComponent");
}

bool SceneScriptRuntime::pushScriptEnvironment(ScriptComponent* component) const {
    if (!luaState || !component) {
        return false;
    }

    lua_pushlightuserdata(luaState, component);
    lua_gettable(luaState, LUA_REGISTRYINDEX);
    return lua_istable(luaState, -1);
}

bool SceneScriptRuntime::pushScriptFunction(ScriptComponent* component, const std::string& name) const {
    if (!pushScriptEnvironment(component)) {
        return false;
    }

    lua_getfield(luaState, -1, name.c_str());
    lua_remove(luaState, -2);
    return lua_isfunction(luaState, -1);
}

bool SceneScriptRuntime::loadScript(ScriptComponent* component, const std::string& path) {
    if (!component || path.empty()) {
        return false;
    }

    if (!ensureInitialized()) {
        return false;
    }

    unloadScript(component);
    setActiveComponent(component);

    lua_newtable(luaState);
    lua_newtable(luaState);
    lua_pushglobaltable(luaState);
    lua_setfield(luaState, -2, "__index");
    lua_setmetatable(luaState, -2);
    const int envIndex = lua_gettop(luaState);

    if (luaL_loadfile(luaState, AssetPaths::resolve(path).c_str()) != LUA_OK) {
        const char* error = lua_tostring(luaState, -1);
#ifdef VITA_BUILD
        printf("SceneScriptRuntime: Failed to load %s: %s\n", path.c_str(), error ? error : "unknown error");
#else
        (void)error;
#endif
        lua_pop(luaState, 2);
        return false;
    }

    lua_pushvalue(luaState, envIndex);
    if (lua_setupvalue(luaState, -2, 1) == nullptr) {
        lua_pop(luaState, 1);
    }

    if (lua_pcall(luaState, 0, 0, 0) != LUA_OK) {
        const char* error = lua_tostring(luaState, -1);
#ifdef VITA_BUILD
        printf("SceneScriptRuntime: Failed to run %s: %s\n", path.c_str(), error ? error : "unknown error");
#else
        (void)error;
#endif
        lua_pop(luaState, 2);
        return false;
    }

    lua_pushlightuserdata(luaState, component);
    lua_pushvalue(luaState, envIndex);
    lua_settable(luaState, LUA_REGISTRYINDEX);
    lua_pop(luaState, 1);
    return true;
}

void SceneScriptRuntime::unloadScript(ScriptComponent* component) {
    if (!luaState || !component) {
        return;
    }

    lua_pushlightuserdata(luaState, component);
    lua_pushnil(luaState);
    lua_settable(luaState, LUA_REGISTRYINDEX);
}

bool SceneScriptRuntime::hasFunction(ScriptComponent* component, const std::string& name) const {
    if (!pushScriptFunction(component, name)) {
        if (luaState) {
            lua_pop(luaState, 1);
        }
        return false;
    }

    lua_pop(luaState, 1);
    return true;
}

bool SceneScriptRuntime::callFunction(ScriptComponent* component, const std::string& name) {
    if (!luaState || !component) {
        return false;
    }

    setActiveComponent(component);

    if (!pushScriptFunction(component, name)) {
        if (lua_isnoneornil(luaState, -1)) {
            lua_pop(luaState, 1);
        }
        return false;
    }

    if (lua_pcall(luaState, 0, 0, 0) != LUA_OK) {
        const char* error = lua_tostring(luaState, -1);
#ifdef VITA_BUILD
        printf("SceneScriptRuntime: Error in %s(): %s\n", name.c_str(), error ? error : "unknown error");
#else
        std::cerr << "SceneScriptRuntime: Error in " << name << "(): " << (error ? error : "unknown error") << std::endl;
#endif
        lua_pop(luaState, 1);
        return false;
    }

    return true;
}

bool SceneScriptRuntime::callFunction(ScriptComponent* component, const std::string& name, float param) {
    if (!luaState || !component) {
        return false;
    }

    setActiveComponent(component);

    if (!pushScriptFunction(component, name)) {
        if (lua_isnoneornil(luaState, -1)) {
            lua_pop(luaState, 1);
        }
        return false;
    }

    lua_pushnumber(luaState, param);
    if (lua_pcall(luaState, 1, 0, 0) != LUA_OK) {
        const char* error = lua_tostring(luaState, -1);
#ifdef VITA_BUILD
        printf("SceneScriptRuntime: Error in %s(): %s\n", name.c_str(), error ? error : "unknown error");
#else
        std::cerr << "SceneScriptRuntime: Error in " << name << "(): " << (error ? error : "unknown error") << std::endl;
#endif
        lua_pop(luaState, 1);
        return false;
    }

    return true;
}

}
