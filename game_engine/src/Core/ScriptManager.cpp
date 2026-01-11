#include "Core/ScriptManager.h"
#include "Input/InputManager.h"
#include "Physics/PhysicsManager.h"
#include "Rendering/Renderer.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Core/Engine.h"
#include "Core/MenuManager.h"
#include <iostream>
#include <ctime>
#include <cmath>
#include <cstring>
#ifndef VITA_BUILD
#include <filesystem>
#include <chrono>
#endif

#ifdef LINUX_BUILD
    #include <GLFW/glfw3.h>
#endif

// Lua includes
extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace GameEngine {

std::unique_ptr<ScriptManager> ScriptManager::instance = nullptr;

ScriptManager& ScriptManager::getInstance() {
    if (!instance) {
        instance = std::unique_ptr<ScriptManager>(new ScriptManager());
    }
    return *instance;
}

ScriptManager::ScriptManager()
    : globalLuaState(nullptr)
    , scriptDirectory("scripts/")
    , hotReloadEnabled(false)
    , initialized(false)
{
    std::cout << "ScriptManager: Constructor called" << std::endl;
}

ScriptManager::~ScriptManager() {
    shutdown();
}

bool ScriptManager::initialize() {
    if (initialized) {
        return true;
    }
    
#ifdef VITA_BUILD
    printf("ScriptManager: Initializing...\n");
#else
    std::cout << "ScriptManager: Initializing..." << std::endl;
#endif
    
    if (!initializeLua()) {
        return false;
    }
    
    bindEngineSystems();
    
    initialized = true;
    std::cout << "ScriptManager: Successfully initialized" << std::endl;
    return true;
}

void ScriptManager::shutdown() {
    if (!initialized) {
        return;
    }
    
    std::cout << "ScriptManager: Shutting down..." << std::endl;
    
    cleanupLua();
    
    initialized = false;
    std::cout << "ScriptManager: Shutdown complete" << std::endl;
}

bool ScriptManager::executeScript(const std::string& scriptPath) {
    if (!globalLuaState || !initialized) {
        std::cerr << "ScriptManager: Cannot execute script - not initialized" << std::endl;
        return false;
    }
    
    std::cout << "ScriptManager: Executing script: " << scriptPath << std::endl;
    
    int result = luaL_dofile(globalLuaState, scriptPath.c_str());
    if (result != LUA_OK) {
        handleLuaError("executeScript: " + scriptPath);
        return false;
    }
    
    // Watch file for hot reload if enabled
    if (hotReloadEnabled) {
        watchScriptFile(scriptPath);
    }
    
    return true;
}

bool ScriptManager::executeScriptString(const std::string& scriptCode) {
    if (!globalLuaState || !initialized) {
        std::cerr << "ScriptManager: Cannot execute script string - not initialized" << std::endl;
        return false;
    }
    
    int result = luaL_dostring(globalLuaState, scriptCode.c_str());
    if (result != LUA_OK) {
        handleLuaError("executeScriptString");
        return false;
    }
    
    return true;
}

void ScriptManager::hotReloadScript(const std::string& scriptPath) {
    if (!hotReloadEnabled) {
        return;
    }
    
    if (!hasFileChanged(scriptPath)) {
        return;
    }
    
    std::cout << "ScriptManager: Hot reloading script: " << scriptPath << std::endl;
    
    executeScript(scriptPath);
    
    scriptFileTimes[scriptPath] = getFileModificationTime(scriptPath);
}

void ScriptManager::hotReloadAllScripts() {
    if (!hotReloadEnabled) {
        return;
    }
    
    for (auto& pair : scriptFileTimes) {
        hotReloadScript(pair.first);
    }
}

void ScriptManager::watchScriptFile(const std::string& scriptPath) {
    if (!hotReloadEnabled) {
        return;
    }
    
    scriptFileTimes[scriptPath] = getFileModificationTime(scriptPath);
    std::cout << "ScriptManager: Watching script file: " << scriptPath << std::endl;
}

void ScriptManager::unwatchScriptFile(const std::string& scriptPath) {
    auto it = scriptFileTimes.find(scriptPath);
    if (it != scriptFileTimes.end()) {
        scriptFileTimes.erase(it);
        std::cout << "ScriptManager: Stopped watching script file: " << scriptPath << std::endl;
    }
}

void ScriptManager::bindEngineSystems() {
    if (!globalLuaState) {
        return;
    }
    
#ifdef VITA_BUILD
    printf("ScriptManager: Binding engine systems to Lua...\n");
#else
    std::cout << "ScriptManager: Binding engine systems to Lua..." << std::endl;
#endif
    
    bindCommonFunctions();
    bindMathFunctions();
    bindUtilityFunctions();
    bindInputSystem();
    bindPhysicsSystem();
    bindRendererSystem();
    bindSceneSystem();
    bindPickupZoneSystem();
    bindMenuSystem();
    
    std::cout << "ScriptManager: Engine systems bound to Lua" << std::endl;
}

void ScriptManager::setErrorCallback(std::function<void(const std::string&)> callback) {
    errorCallback = callback;
}

bool ScriptManager::initializeLua() {
    globalLuaState = luaL_newstate();
    if (!globalLuaState) {
        std::cerr << "ScriptManager: Failed to create global Lua state" << std::endl;
        return false;
    }
    
    luaL_openlibs(globalLuaState);
    
    lua_atpanic(globalLuaState, luaErrorHandler);
    
    std::cout << "ScriptManager: Lua state initialized" << std::endl;
    return true;
}

void ScriptManager::cleanupLua() {
    if (globalLuaState) {
        lua_close(globalLuaState);
        globalLuaState = nullptr;
    }
    
    scriptFileTimes.clear();
}

void ScriptManager::bindCommonFunctions() {
    if (!globalLuaState) {
        return;
    }
    
    // Print function
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* message = lua_tostring(L, 1);
        if (message) {
#ifdef VITA_BUILD
            // Use printf for Vita as std::cout might not work
            printf("Lua: %s\n", message);
#else
            std::cout << "Lua: " << message << std::endl;
#endif
        }
        return 0;
    });
    lua_setglobal(globalLuaState, "print");
    
    // Time functions
#ifndef VITA_BUILD
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
        lua_pushnumber(L, seconds);
        return 1;
    });
    lua_setglobal(globalLuaState, "getTime");
#else
    // Vita build - simplified time function
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        lua_pushnumber(L, 0.0); // Placeholder time
        return 1;
    });
    lua_setglobal(globalLuaState, "getTime");
#endif
    
    lua_newtable(globalLuaState);
    lua_pushnumber(globalLuaState, 0);
    lua_setfield(globalLuaState, -2, "score");
    lua_setglobal(globalLuaState, "gameState");
    
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
#ifndef VITA_BUILD
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
#else
        double seconds = 0.0;
#endif
        
        float x = std::sin(seconds * 0.5) * 3;
        float y = 2 + std::sin(seconds * 2) * 0.5;
        float z = std::cos(seconds * 0.3) * 2;
        
        lua_pushnumber(L, x);
        lua_pushnumber(L, y);
        lua_pushnumber(L, z);
        return 3;
    });
    lua_setglobal(globalLuaState, "getCameraPosition");
    
}

void ScriptManager::bindMathFunctions() {
    if (!globalLuaState) {
        return;
    }
    
    lua_newtable(globalLuaState);
    
    lua_pushstring(globalLuaState, "vec3");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        float x = luaL_optnumber(L, 1, 0.0);
        float y = luaL_optnumber(L, 2, 0.0);
        float z = luaL_optnumber(L, 3, 0.0);
        
        lua_newtable(L);
        lua_pushstring(L, "x");
        lua_pushnumber(L, x);
        lua_settable(L, -3);
        lua_pushstring(L, "y");
        lua_pushnumber(L, y);
        lua_settable(L, -3);
        lua_pushstring(L, "z");
        lua_pushnumber(L, z);
        lua_settable(L, -3);
        
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_setglobal(globalLuaState, "math");
}

void ScriptManager::bindUtilityFunctions() {
    if (!globalLuaState) {
        return;
    }
    
    lua_newtable(globalLuaState);
    
    // Random functions
    lua_pushstring(globalLuaState, "random");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        float min = luaL_optnumber(L, 1, 0.0);
        float max = luaL_optnumber(L, 2, 1.0);
        float result = min + (max - min) * (rand() / (float)RAND_MAX);
        lua_pushnumber(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_setglobal(globalLuaState, "util");
}

void ScriptManager::bindInputSystem() {
    if (!globalLuaState) {
        std::cout << "ScriptManager: ERROR - globalLuaState is null, cannot bind input system!" << std::endl;
        return;
    }
    
#ifdef VITA_BUILD
    printf("ScriptManager: Binding input system to Lua...\n");
#else
    std::cout << "ScriptManager: Binding input system to Lua..." << std::endl;
#endif
    
    lua_newtable(globalLuaState);
    
    // Action-based input functions
    lua_pushstring(globalLuaState, "isActionPressed");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isActionPressed(actionName);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isActionHeld");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        std::cout << "ScriptManager: isActionHeld called for action: " << actionName << std::endl;
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isActionHeld(actionName);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isActionReleased");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isActionReleased(actionName);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getActionAxis");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        std::cout << "ScriptManager: getActionAxis called for action: " << actionName << std::endl;
        auto& inputManager = GetEngine().getInputManager();
        float result = inputManager.getActionAxis(actionName);
        lua_pushnumber(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getActionVector2");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 result = inputManager.getActionVector2(actionName);
        lua_pushnumber(L, result.x);
        lua_pushnumber(L, result.y);
        return 2;
    });
    lua_settable(globalLuaState, -3);
    
    // Legacy direct input functions (for backward compatibility)
    lua_pushstring(globalLuaState, "isKeyPressed");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* keyName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        
#ifdef LINUX_BUILD
        // Map string keys to GLFW codes
        int keyCode = 0;
        if (strcmp(keyName, "W") == 0) keyCode = GLFW_KEY_W;
        else if (strcmp(keyName, "A") == 0) keyCode = GLFW_KEY_A;
        else if (strcmp(keyName, "S") == 0) keyCode = GLFW_KEY_S;
        else if (strcmp(keyName, "D") == 0) keyCode = GLFW_KEY_D;
        else if (strcmp(keyName, "SPACE") == 0) keyCode = GLFW_KEY_SPACE;
        else if (strcmp(keyName, "SHIFT") == 0) keyCode = GLFW_KEY_LEFT_SHIFT;
        else if (strcmp(keyName, "LEFT") == 0) keyCode = GLFW_KEY_LEFT;
        else if (strcmp(keyName, "RIGHT") == 0) keyCode = GLFW_KEY_RIGHT;
        else if (strcmp(keyName, "UP") == 0) keyCode = GLFW_KEY_UP;
        else if (strcmp(keyName, "DOWN") == 0) keyCode = GLFW_KEY_DOWN;
        
        bool result = inputManager.isKeyPressed(keyCode);
        lua_pushboolean(L, result);
#else
        lua_pushboolean(L, false);
#endif
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isKeyDown");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* keyName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        
#ifdef LINUX_BUILD
        // Map string keys to GLFW codes
        int keyCode = 0;
        if (strcmp(keyName, "W") == 0) keyCode = GLFW_KEY_W;
        else if (strcmp(keyName, "A") == 0) keyCode = GLFW_KEY_A;
        else if (strcmp(keyName, "S") == 0) keyCode = GLFW_KEY_S;
        else if (strcmp(keyName, "D") == 0) keyCode = GLFW_KEY_D;
        else if (strcmp(keyName, "SPACE") == 0) keyCode = GLFW_KEY_SPACE;
        else if (strcmp(keyName, "SHIFT") == 0) keyCode = GLFW_KEY_LEFT_SHIFT;
        else if (strcmp(keyName, "LEFT") == 0) keyCode = GLFW_KEY_LEFT;
        else if (strcmp(keyName, "RIGHT") == 0) keyCode = GLFW_KEY_RIGHT;
        else if (strcmp(keyName, "UP") == 0) keyCode = GLFW_KEY_UP;
        else if (strcmp(keyName, "DOWN") == 0) keyCode = GLFW_KEY_DOWN;
        else if (strcmp(keyName, "ENTER") == 0 || strcmp(keyName, "Return") == 0) keyCode = GLFW_KEY_ENTER;
        
        bool result = inputManager.isKeyHeld(keyCode);
        lua_pushboolean(L, result);
#else
        lua_pushboolean(L, false);
#endif
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    // Vita-specific functions
    lua_pushstring(globalLuaState, "isButtonPressed");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        uint32_t button = (uint32_t)luaL_checkinteger(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isButtonPressed(button);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getLeftStick");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 stick = inputManager.getLeftStick();
        lua_pushnumber(L, stick.x);
        lua_pushnumber(L, stick.y);
        return 2;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getRightStick");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 stick = inputManager.getRightStick();
        lua_pushnumber(L, stick.x);
        lua_pushnumber(L, stick.y);
        return 2;
    });
    lua_settable(globalLuaState, -3);
    
#ifdef LINUX_BUILD
    // Mouse input functions (Linux only)
    lua_pushstring(globalLuaState, "setMouseCapture");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        bool enabled = lua_toboolean(L, 1) != 0;
        auto& inputManager = GetEngine().getInputManager();
        inputManager.setMouseCapture(enabled);
        return 0;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isMouseCaptured");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isMouseCaptured();
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "setDebugMouseInput");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        bool enabled = lua_toboolean(L, 1) != 0;
        auto& inputManager = GetEngine().getInputManager();
        inputManager.setDebugMouseInput(enabled);
        std::cout << "Lua: setDebugMouseInput(" << (enabled ? "true" : "false") << ")" << std::endl;
        return 0;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getMousePosition");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 pos = inputManager.getMousePosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        return 2;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getMouseDelta");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 delta = inputManager.getMouseDelta();
        lua_pushnumber(L, delta.x);
        lua_pushnumber(L, delta.y);
        return 2;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isMouseButtonPressed");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* buttonName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        
        // Map button names to GLFW button codes
        int button = GLFW_MOUSE_BUTTON_LEFT; // Default to left button
        if (strcmp(buttonName, "LEFT") == 0 || strcmp(buttonName, "0") == 0) {
            button = GLFW_MOUSE_BUTTON_LEFT;
        } else if (strcmp(buttonName, "RIGHT") == 0 || strcmp(buttonName, "1") == 0) {
            button = GLFW_MOUSE_BUTTON_RIGHT;
        } else if (strcmp(buttonName, "MIDDLE") == 0 || strcmp(buttonName, "2") == 0) {
            button = GLFW_MOUSE_BUTTON_MIDDLE;
        } else {
            // Try to parse as number
            button = (int)luaL_optnumber(L, 1, GLFW_MOUSE_BUTTON_LEFT);
        }
        
        bool result = inputManager.isMouseButtonPressed(button);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isMouseButtonHeld");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* buttonName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        
        // Map button names to GLFW button codes
        int button = GLFW_MOUSE_BUTTON_LEFT; // Default to left button
        if (strcmp(buttonName, "LEFT") == 0 || strcmp(buttonName, "0") == 0) {
            button = GLFW_MOUSE_BUTTON_LEFT;
        } else if (strcmp(buttonName, "RIGHT") == 0 || strcmp(buttonName, "1") == 0) {
            button = GLFW_MOUSE_BUTTON_RIGHT;
        } else if (strcmp(buttonName, "MIDDLE") == 0 || strcmp(buttonName, "2") == 0) {
            button = GLFW_MOUSE_BUTTON_MIDDLE;
        } else {
            // Try to parse as number
            button = (int)luaL_optnumber(L, 1, GLFW_MOUSE_BUTTON_LEFT);
        }
        
        bool result = inputManager.isMouseButtonHeld(button);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isMouseButtonReleased");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* buttonName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        
        // Map button names to GLFW button codes
        int button = GLFW_MOUSE_BUTTON_LEFT; // Default to left button
        if (strcmp(buttonName, "LEFT") == 0 || strcmp(buttonName, "0") == 0) {
            button = GLFW_MOUSE_BUTTON_LEFT;
        } else if (strcmp(buttonName, "RIGHT") == 0 || strcmp(buttonName, "1") == 0) {
            button = GLFW_MOUSE_BUTTON_RIGHT;
        } else if (strcmp(buttonName, "MIDDLE") == 0 || strcmp(buttonName, "2") == 0) {
            button = GLFW_MOUSE_BUTTON_MIDDLE;
        } else {
            // Try to parse as number
            button = (int)luaL_optnumber(L, 1, GLFW_MOUSE_BUTTON_LEFT);
        }
        
        bool result = inputManager.isMouseButtonReleased(button);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getMouseWheel");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        float wheel = inputManager.getMouseWheel();
        lua_pushnumber(L, wheel);
        return 1;
    });
    lua_settable(globalLuaState, -3);
#endif
    
    lua_setglobal(globalLuaState, "input");
    
    // Quit game function
    lua_pushstring(globalLuaState, "quitGame");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        auto& engine = GetEngine();
        engine.setRunning(false);
        return 0;
    });
    lua_setglobal(globalLuaState, "quitGame");
    
    // Verify the input table was set correctly
    lua_getglobal(globalLuaState, "input");
    if (lua_istable(globalLuaState, -1)) {
#ifdef VITA_BUILD
        printf("ScriptManager: Input table verified as table in global scope\n");
#else
        std::cout << "ScriptManager: Input table verified as table in global scope" << std::endl;
#endif
    } else {
#ifdef VITA_BUILD
        printf("ScriptManager: ERROR - Input table is NOT a table! Type: %s\n", lua_typename(globalLuaState, -1));
#else
        std::cout << "ScriptManager: ERROR - Input table is NOT a table! Type: " << lua_typename(globalLuaState, -1) << std::endl;
#endif
    }
    lua_pop(globalLuaState, 1); // Remove the table from stack
    
#ifdef VITA_BUILD
    printf("ScriptManager: Input system bound to Lua successfully!\n");
#else
    std::cout << "ScriptManager: Input system bound to Lua successfully!" << std::endl;
#endif
}

void ScriptManager::bindPhysicsSystem() {
    if (!globalLuaState) {
        return;
    }
    
    lua_newtable(globalLuaState);
    
    lua_pushstring(globalLuaState, "addForce");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        return 0;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "setVelocity");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        return 0;
    });
    lua_settable(globalLuaState, -3);
    
    lua_setglobal(globalLuaState, "physics");
}

void ScriptManager::bindRendererSystem() {
    if (!globalLuaState) {
        return;
    }
    
    lua_newtable(globalLuaState);
    
    lua_pushstring(globalLuaState, "drawText");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        return 0;
    });
    lua_settable(globalLuaState, -3);
    
    lua_setglobal(globalLuaState, "renderer");
}

void ScriptManager::bindSceneSystem() {
    if (!globalLuaState) {
        return;
    }
    
    lua_newtable(globalLuaState);
    
    lua_pushstring(globalLuaState, "findNode");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        lua_pushnil(L);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "setActiveCamera");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* cameraNodeName = luaL_checkstring(L, 1);
        
        if (!cameraNodeName) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        auto& engine = GetEngine();
        auto& sceneManager = engine.getSceneManager();
        auto activeScene = sceneManager.getCurrentScene();
        
        if (activeScene && cameraNodeName) {
            auto cameraNode = activeScene->findNode(cameraNodeName);
            if (cameraNode) {
                auto cameraComponent = cameraNode->getComponent<CameraComponent>();
                if (cameraComponent) {
                    activeScene->setActiveCamera(cameraNode);
                    lua_pushboolean(L, true);
                    return 1;
                } else {
                    std::cout << "setActiveCamera: Node '" << cameraNodeName << "' does not have a CameraComponent" << std::endl;
                }
            } else {
                std::cout << "setActiveCamera: Camera node '" << cameraNodeName << "' not found" << std::endl;
            }
        }
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "loadScene");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* sceneName = luaL_checkstring(L, 1);
        
        if (!sceneName) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        auto& engine = GetEngine();
        auto& sceneManager = engine.getSceneManager();
        
        // Try to load scene by name
        bool success = sceneManager.loadScene(sceneName);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "loadSceneFromFile");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* sceneName = luaL_checkstring(L, 1);
        const char* filepath = luaL_checkstring(L, 2);
        
        if (!sceneName || !filepath) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        auto& engine = GetEngine();
        auto& sceneManager = engine.getSceneManager();
        
        bool success = sceneManager.loadSceneFromFile(sceneName, filepath);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_setglobal(globalLuaState, "scene");
}

void ScriptManager::bindPickupZoneSystem() {
    if (!globalLuaState) {
        return;
    }
    
    lua_newtable(globalLuaState);
    
    lua_pushstring(globalLuaState, "zones");
    lua_newtable(globalLuaState);
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "createZone");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* name = luaL_checkstring(L, 1);
        float x = luaL_optnumber(L, 2, 0.0);
        float y = luaL_optnumber(L, 3, 0.0);
        float z = luaL_optnumber(L, 4, 0.0);
        float width = luaL_optnumber(L, 5, 1.0);
        float height = luaL_optnumber(L, 6, 1.0);
        float depth = luaL_optnumber(L, 7, 1.0);
        
        lua_newtable(L);
        lua_pushstring(L, "name");
        lua_pushstring(L, name);
        lua_settable(L, -3);
        
        lua_pushstring(L, "x");
        lua_pushnumber(L, x);
        lua_settable(L, -3);
        
        lua_pushstring(L, "y");
        lua_pushnumber(L, y);
        lua_settable(L, -3);
        
        lua_pushstring(L, "z");
        lua_pushnumber(L, z);
        lua_settable(L, -3);
        
        lua_pushstring(L, "width");
        lua_pushnumber(L, width);
        lua_settable(L, -3);
        
        lua_pushstring(L, "height");
        lua_pushnumber(L, height);
        lua_settable(L, -3);
        
        lua_pushstring(L, "depth");
        lua_pushnumber(L, depth);
        lua_settable(L, -3);
        
        lua_pushstring(L, "objects");
        lua_newtable(L);
        lua_settable(L, -3);
        
        // Store in global zones table
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, name);
        lua_pushvalue(L, -4); // Copy the zone table
        lua_settable(L, -3);
        lua_pop(L, 2); // Remove zones table and pickupZone table
        
        return 1; // Return the zone table
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "isObjectInZone");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* zoneName = luaL_checkstring(L, 1);
        const char* objectName = luaL_checkstring(L, 2);
        
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 3); // Clean up stack
            lua_pushboolean(L, false);
            return 1;
        }
        
        lua_pushstring(L, "objects");
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 4);
            lua_pushboolean(L, false);
            return 1;
        }
        
        lua_pushstring(L, objectName);
        lua_gettable(L, -2);
        bool isInZone = !lua_isnil(L, -1);
        lua_pop(L, 5); // Clean up stack
        
        lua_pushboolean(L, isInZone);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "addObjectToZone");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* zoneName = luaL_checkstring(L, 1);
        const char* objectName = luaL_checkstring(L, 2);
        
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 3); // Clean up stack
            lua_pushboolean(L, false);
            return 1;
        }
        
        lua_pushstring(L, "objects");
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 4);
            lua_pushboolean(L, false);
            return 1;
        }
        
        lua_pushstring(L, objectName);
        lua_pushboolean(L, true);
        lua_settable(L, -3);
        
        lua_pop(L, 4); // Clean up stack
        lua_pushboolean(L, true);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "removeObjectFromZone");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* zoneName = luaL_checkstring(L, 1);
        const char* objectName = luaL_checkstring(L, 2);
        
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 3); // Clean up stack
            lua_pushboolean(L, false);
            return 1;
        }
        
        lua_pushstring(L, "objects");
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 4);
            lua_pushboolean(L, false);
            return 1;
        }
        
        lua_pushstring(L, objectName);
        lua_pushnil(L);
        lua_settable(L, -3);
        
        lua_pop(L, 4); // Clean up stack
        lua_pushboolean(L, true);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_pushstring(globalLuaState, "getObjectsInZone");
    lua_pushcfunction(globalLuaState, [](lua_State* L) -> int {
        const char* zoneName = luaL_checkstring(L, 1);
        
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 3);
            lua_newtable(L);
            return 1;
        }
        
        lua_pushstring(L, "objects");
        lua_gettable(L, -2);
        
        if (lua_isnil(L, -1)) {
            lua_pop(L, 4);
            lua_newtable(L);
            return 1;
        }
        
        lua_pop(L, 3);
        return 1;
    });
    lua_settable(globalLuaState, -3);
    
    lua_setglobal(globalLuaState, "pickupZone");
}

void ScriptManager::bindMenuSystem() {
    if (!globalLuaState) {
        return;
    }
    
    // Bind MenuManager to Lua
    // MenuManager has its own bindToLua method that sets up all the bindings
    MenuManager::getInstance().bindToLua(globalLuaState);
    
    std::cout << "ScriptManager: Menu system bound to Lua" << std::endl;
}

time_t ScriptManager::getFileModificationTime(const std::string& filePath) {
#ifndef VITA_BUILD
    try {
        auto fileTime = std::filesystem::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        return std::chrono::system_clock::to_time_t(sctp);
    } catch (const std::filesystem::filesystem_error& ex) {
        std::cerr << "ScriptManager: Error getting file modification time: " << ex.what() << std::endl;
        return 0;
    }
#else
    // Vita build - simplified file modification time
    return 0; // Always return 0 for Vita (no file watching)
#endif
}

bool ScriptManager::hasFileChanged(const std::string& filePath) {
    auto it = scriptFileTimes.find(filePath);
    if (it == scriptFileTimes.end()) {
        return true; // File not watched yet
    }
    
    time_t currentTime = getFileModificationTime(filePath);
    return currentTime > it->second;
}

void ScriptManager::handleLuaError(const std::string& operation) {
    if (!globalLuaState) {
        std::cerr << "ScriptManager: Lua error in " << operation << " - No Lua state" << std::endl;
        return;
    }
    
    const char* errorMsg = lua_tostring(globalLuaState, -1);
    if (errorMsg) {
        std::string error = "ScriptManager: Lua error in " + operation + ": " + errorMsg;
        std::cerr << error << std::endl;
        
        if (errorCallback) {
            errorCallback(error);
        }
        
        lua_pop(globalLuaState, 1); // Remove error message
    } else {
        std::string error = "ScriptManager: Unknown Lua error in " + operation;
        std::cerr << error << std::endl;
        
        if (errorCallback) {
            errorCallback(error);
        }
    }
}

int ScriptManager::luaErrorHandler(lua_State* L) {
    const char* errorMsg = lua_tostring(L, -1);
    if (errorMsg) {
        std::cerr << "ScriptManager: Lua panic: " << errorMsg << std::endl;
    } else {
        std::cerr << "ScriptManager: Lua panic: Unknown error" << std::endl;
    }
    return 0;
}

} // namespace GameEngine
