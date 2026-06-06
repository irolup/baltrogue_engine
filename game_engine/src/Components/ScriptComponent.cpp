#include "Components/ScriptComponent.h"
#include "Core/ScriptManager.h"
#include "Core/Engine.h"
#include "Core/MenuManager.h"
#include "Components/TextComponent.h"
#include "Components/Area3DComponent.h"
#include "Components/PhysicsComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/SoundComponent.h"
#include "Components/SkyboxComponent.h"
#include "Components/JointComponent.h"
#include "Components/BeamRenderer.h"
#include "Components/MeshRenderer.h"
#include "Components/RaycastComponent.h"
#include "Navigation/NavGrid.h"
#include "Navigation/NavGridRegistry.h"
#include "Components/NavVolumeComponent.h"
#include "Components/NavAgentComponent.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/AnimationManager.h"
#include "Scene/SceneNode.h"
#include "Scene/Scene.h"
#include "Rendering/Mesh.h"
#include "Input/InputManager.h"
#include "Physics/PhysicsManager.h"
#include "Rendering/Renderer.h"
#include "Components/CameraComponent.h"
#include "Core/Transform.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <functional>

#ifdef LINUX_BUILD
#include <GLFW/glfw3.h>
#endif

// Lua includes
extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

// Static map for editor UI buffers - declared at namespace level to persist
static std::map<ScriptComponent*, std::string> scriptPathBuffers;

ScriptComponent::ScriptComponent()
    : luaState(nullptr)
    , scriptLoaded(false)
    , scriptStarted(false)
    , pauseExempt(false)
{
    // Constructor debug output removed
}

ScriptComponent::~ScriptComponent() {
    cleanupLuaState();
    
#ifdef EDITOR_BUILD
    scriptPathBuffers.erase(this);
#endif
}

void ScriptComponent::start() {
    if (scriptPath.empty()) {
        // No script path set - this is allowed (component exists but no script assigned)
        return;
    }
    
#ifdef VITA_BUILD
    printf("ScriptComponent::start() called for script: %s\n", scriptPath.c_str());
#else
    std::cout << "ScriptComponent::start() called for script: " << scriptPath << std::endl;
#endif
    
    if (!scriptLoaded || !luaState) {
#ifdef VITA_BUILD
        printf("ScriptComponent: Cannot start - script not loaded or no lua state\n");
#else
        std::cout << "ScriptComponent: Cannot start - script not loaded or no lua state" << std::endl;
#endif
        return;
    }
    
    if (scriptStarted) {
#ifdef VITA_BUILD
        printf("ScriptComponent: Script already started, skipping start() call\n");
#else
        std::cout << "ScriptComponent: Script already started, skipping start() call" << std::endl;
#endif
        return;
    }
    
    if (hasScriptFunction("start")) {
#ifdef VITA_BUILD
        printf("ScriptComponent: Found start function, calling it\n");
#else
        std::cout << "ScriptComponent: Found start function, calling it" << std::endl;
#endif
        callScriptFunction("start");
        scriptStarted = true;
        
        if (luaState) {
            const char* error = lua_tostring(luaState, -1);
            if (error) {
#ifdef VITA_BUILD
                printf("ScriptComponent: Lua error in start(): %s\n", error);
#else
                std::cout << "ScriptComponent: Lua error in start(): " << error << std::endl;
#endif
                lua_pop(luaState, 1); // Remove error message from stack
            }
        }
#ifdef VITA_BUILD
        printf("ScriptComponent: Called start() for script: %s\n", scriptPath.c_str());
#else
        std::cout << "ScriptComponent: Called start() for script: " << scriptPath << std::endl;
#endif
    } else {
#ifdef VITA_BUILD
        printf("ScriptComponent: No start function found in script: %s\n", scriptPath.c_str());
#else
        std::cout << "ScriptComponent: No start function found in script: " << scriptPath << std::endl;
#endif
    }
}

void ScriptComponent::update(float deltaTime) {
    if (!scriptLoaded || !luaState || !scriptStarted) {
        return;
    }
    
    // Debug: Print if this is a pause-exempt script being updated while paused
    static int debugFrameCount = 0;
    if (pauseExempt && MenuManager::getInstance().isGamePaused()) {
        if (++debugFrameCount % 60 == 0) {  // Print every 60 frames
            std::cout << "ScriptComponent: Updating pause-exempt script: " << scriptPath 
                      << " (pauseExempt=" << (pauseExempt ? "true" : "false") << ")" << std::endl;
        }
    }
    
    if (hasScriptFunction("update")) {
        callScriptFunction("update", deltaTime);
    }
}

void ScriptComponent::fixedUpdate(float deltaTime) {
    if (!scriptLoaded || !luaState || !scriptStarted) {
        return;
    }
    bool paused = MenuManager::getInstance().isGamePaused();
    if (paused && !pauseExempt) {
        return;
    }
    if (hasScriptFunction("fixedUpdate")) {
        callScriptFunction("fixedUpdate", deltaTime);
    }
}

void ScriptComponent::lateUpdate(float deltaTime) {
    if (!scriptLoaded || !luaState || !scriptStarted) {
        return;
    }
    bool paused = MenuManager::getInstance().isGamePaused();
    if (paused && !pauseExempt) {
        return;
    }
    if (hasScriptFunction("lateUpdate")) {
        callScriptFunction("lateUpdate", deltaTime);
    }
}

void ScriptComponent::render(IRenderer& renderer) {
    if (!scriptLoaded || !luaState || !scriptStarted) {
        return;
    }
    
    if (hasScriptFunction("render")) {
        callScriptFunction("render");
    }
}

void ScriptComponent::destroy() {
    if (!scriptLoaded || !luaState) {
        return;
    }
    
    if (hasScriptFunction("destroy")) {
        callScriptFunction("destroy");
        std::cout << "ScriptComponent: Called destroy() for script: " << scriptPath << std::endl;
    }
    
    scriptStarted = false;
}

bool ScriptComponent::loadScript(const std::string& scriptPath) {
    if (scriptPath.empty()) {
        std::cerr << "ScriptComponent: Empty script path provided" << std::endl;
        return false;
    }
    
    cleanupLuaState();
    
    if (!initializeLuaState()) {
        return false;
    }
    
    int result = luaL_dofile(luaState, scriptPath.c_str());
    if (result != LUA_OK) {
        handleLuaError("loadScript");
        cleanupLuaState();
        return false;
    }
    
    this->scriptPath = scriptPath;
    scriptLoaded = true;
    scriptStarted = false;
    
    std::cout << "ScriptComponent: Successfully loaded script: " << scriptPath 
              << " (pauseExempt=" << (pauseExempt ? "true" : "false") << ")" << std::endl;
    return true;
}

void ScriptComponent::reloadScript() {
    if (scriptPath.empty()) {
        return;
    }
    
    std::cout << "ScriptComponent: Reloading script: " << scriptPath << std::endl;
    loadScript(scriptPath);
}

void ScriptComponent::setScriptPath(const std::string& path) {
    if (path != scriptPath) {
        loadScript(path);
    }
}

void ScriptComponent::callScriptFunction(const std::string& functionName) {
    if (!luaState || !scriptLoaded) {
        return;
    }
    
    // Get the function from Lua
    lua_getglobal(luaState, functionName.c_str());
    if (!lua_isfunction(luaState, -1)) {
        lua_pop(luaState, 1); // Remove non-function value
        return;
    }
    
    // Call the function
    int result = lua_pcall(luaState, 0, 0, 0);
    if (result != LUA_OK) {
        handleLuaError("callScriptFunction: " + functionName);
    }
}

void ScriptComponent::callScriptFunction(const std::string& functionName, float param) {
    if (!luaState || !scriptLoaded) {
        return;
    }
    
    // Get the function from Lua
    lua_getglobal(luaState, functionName.c_str());
    if (!lua_isfunction(luaState, -1)) {
        lua_pop(luaState, 1); // Remove non-function value
        return;
    }
    
    // Push parameter
    lua_pushnumber(luaState, param);
    
    // Call the function
    int result = lua_pcall(luaState, 1, 0, 0);
    if (result != LUA_OK) {
        handleLuaError("callScriptFunction: " + functionName);
    }
}

void ScriptComponent::setScriptProperty(const std::string& name, const std::string& value) {
    if (!luaState || !scriptLoaded) {
        return;
    }
    
    lua_pushstring(luaState, value.c_str());
    lua_setglobal(luaState, name.c_str());
}

std::string ScriptComponent::getScriptProperty(const std::string& name) const {
    if (!luaState || !scriptLoaded) {
        return "";
    }
    
    lua_getglobal(luaState, name.c_str());
    if (lua_isstring(luaState, -1)) {
        std::string value = lua_tostring(luaState, -1);
        lua_pop(luaState, 1);
        return value;
    }
    
    lua_pop(luaState, 1);
    return "";
}

bool ScriptComponent::hasScriptFunction(const std::string& functionName) {
    if (!luaState || !scriptLoaded) {
        return false;
    }
    
    lua_getglobal(luaState, functionName.c_str());
    bool isFunction = lua_isfunction(luaState, -1);
    lua_pop(luaState, 1);
    return isFunction;
}

void ScriptComponent::handleLuaError(const std::string& operation) {
    if (!luaState) {
        std::cerr << "ScriptComponent: Lua error in " << operation << " - No Lua state" << std::endl;
        return;
    }
    
    const char* errorMsg = lua_tostring(luaState, -1);
    if (errorMsg) {
        std::cerr << "ScriptComponent: Lua error in " << operation << ": " << errorMsg << std::endl;
        lua_pop(luaState, 1); // Remove error message
    } else {
        std::cerr << "ScriptComponent: Unknown Lua error in " << operation << std::endl;
    }
}

bool ScriptComponent::initializeLuaState() {
    luaState = luaL_newstate();
    if (!luaState) {
        std::cerr << "ScriptComponent: Failed to create Lua state" << std::endl;
        return false;
    }
    
    // Open standard libraries
    luaL_openlibs(luaState);
    
    // Bind engine systems to Lua
    bindEngineToLua();
    
    return true;
}

void ScriptComponent::cleanupLuaState() {
    if (luaState) {
        lua_close(luaState);
        luaState = nullptr;
    }
    scriptLoaded = false;
    scriptStarted = false;
}

void ScriptComponent::bindEngineToLua() {
    if (!luaState) {
        return;
    }

    // Bind common functions first
    bindCommonFunctions();

    // Bind transform with simple approach
    bindTransformToLua();
    
    // Bind pickup zone system
    bindPickupZoneToLua();
    
    // Bind Area3D component system
    bindArea3DToLua();
    
    bindInputToLua();
    bindCameraToLua();
    bindPhysicsToLua();
    bindNavToLua();
    bindRendererToLua();
    bindSceneToLua();
    bindAnimationToLua();
    bindSoundToLua();
    bindSkyboxToLua();
    
    // Bind MenuManager
    MenuManager::getInstance().bindToLua(luaState);
}

void ScriptComponent::bindTransformToLua() {
    if (!luaState || !owner) {
        return;
    }
    
    // Store this component pointer in a global variable for the functions to access
    lua_pushlightuserdata(luaState, this);
    lua_setglobal(luaState, "_currentScriptComponent");
    
    // Use global functions without upvalues
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (self && self->owner && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
            float x = lua_tonumber(L, 1);
            float y = lua_tonumber(L, 2);
            float z = lua_tonumber(L, 3);
            self->owner->getTransform().setPosition(glm::vec3(x, y, z));
            for (size_t i = 0; i < self->owner->getChildCount(); ++i) {
                auto child = self->owner->getChild(i);
                if (child) {
                    auto* phys = child->getComponent<PhysicsComponent>();
                    if (phys) phys->syncTransformToPhysics();
                }
            }
        }
        return 0;
    });
    lua_setglobal(luaState, "setPosition");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (self && self->owner && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
            float x = lua_tonumber(L, 1);
            float y = lua_tonumber(L, 2);
            float z = lua_tonumber(L, 3);
            self->owner->getTransform().setEulerAngles(glm::vec3(x, y, z));
        }
        return 0;
    });
    lua_setglobal(luaState, "setRotation");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        
        if (self && self->owner) {
            lua_newtable(L);
            
            lua_pushstring(L, "getName");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner) {
                    lua_pushstring(L, self->owner->getName().c_str());
                    return 1;
                }
                lua_pushnil(L);
                return 1;
            });
            lua_settable(L, -3);
            
            lua_pushstring(L, "move");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
                    float x = lua_tonumber(L, 1);
                    float y = lua_tonumber(L, 2);
                    float z = lua_tonumber(L, 3);
                    
                    glm::vec3 currentPos = self->owner->getTransform().getPosition();
                    self->owner->getTransform().setPosition(currentPos + glm::vec3(x, y, z));
                }
                return 0;
            });
            lua_settable(L, -3);
            
            lua_pushstring(L, "setPosition");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
                    float x = lua_tonumber(L, 1);
                    float y = lua_tonumber(L, 2);
                    float z = lua_tonumber(L, 3);
                    self->owner->getTransform().setPosition(glm::vec3(x, y, z));
                }
                return 0;
            });
            lua_settable(L, -3);
            
            lua_pushstring(L, "getPosition");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner) {
                    glm::vec3 pos = self->owner->getTransform().getPosition();
                    lua_pushnumber(L, pos.x);
                    lua_pushnumber(L, pos.y);
                    lua_pushnumber(L, pos.z);
                    return 3;
                }
                lua_pushnil(L);
                lua_pushnil(L);
                lua_pushnil(L);
                return 3;
            });
            lua_settable(L, -3);
            
            return 1;
        }
        lua_pushnil(L);
        return 1;
    });
    lua_setglobal(luaState, "node");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        
        if (self && self->owner && self->owner->getParent()) {
            lua_newtable(L);
            
            lua_pushstring(L, "getName");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner && self->owner->getParent()) {
                    lua_pushstring(L, self->owner->getParent()->getName().c_str());
                    return 1;
                }
                lua_pushnil(L);
                return 1;
            });
            lua_settable(L, -3);
            
            lua_pushstring(L, "move");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner && self->owner->getParent() && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
                    float x = lua_tonumber(L, 1);
                    float y = lua_tonumber(L, 2);
                    float z = lua_tonumber(L, 3);
                    
                    glm::vec3 currentPos = self->owner->getParent()->getTransform().getPosition();
                    self->owner->getParent()->getTransform().setPosition(currentPos + glm::vec3(x, y, z));
                }
                return 0;
            });
            lua_settable(L, -3);
            
            lua_pushstring(L, "setPosition");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner && self->owner->getParent() && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
                    float x = lua_tonumber(L, 1);
                    float y = lua_tonumber(L, 2);
                    float z = lua_tonumber(L, 3);
                    self->owner->getParent()->getTransform().setPosition(glm::vec3(x, y, z));
                }
                return 0;
            });
            lua_settable(L, -3);
            
            lua_pushstring(L, "getPosition");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner && self->owner->getParent()) {
                    glm::vec3 pos = self->owner->getParent()->getTransform().getPosition();
                    lua_pushnumber(L, pos.x);
                    lua_pushnumber(L, pos.y);
                    lua_pushnumber(L, pos.z);
                    return 3;
                }
                lua_pushnil(L);
                lua_pushnil(L);
                lua_pushnil(L);
                return 3;
            });
            lua_settable(L, -3);
            
            lua_pushstring(L, "getParent");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getglobal(L, "_currentScriptComponent");
                ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (self && self->owner && self->owner->getParent() && self->owner->getParent()->getParent()) {
                    lua_newtable(L);
                    
                    lua_pushstring(L, "getName");
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        lua_getglobal(L, "_currentScriptComponent");
                        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                        lua_pop(L, 1);
                        if (self && self->owner && self->owner->getParent() && self->owner->getParent()->getParent()) {
                            lua_pushstring(L, self->owner->getParent()->getParent()->getName().c_str());
                            return 1;
                        }
                        lua_pushnil(L);
                        return 1;
                    });
                    lua_settable(L, -3);
                    
                    lua_pushstring(L, "move");
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        lua_getglobal(L, "_currentScriptComponent");
                        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                        lua_pop(L, 1);
                        if (self && self->owner && self->owner->getParent() && self->owner->getParent()->getParent() &&
                            lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
                            auto grandparent = self->owner->getParent()->getParent();
                            glm::vec3 currentPos = grandparent->getTransform().getPosition();
                            grandparent->getTransform().setPosition(currentPos + glm::vec3(
                                lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3)));
                        }
                        return 0;
                    });
                    lua_settable(L, -3);
                    
                    lua_pushstring(L, "setPosition");
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        lua_getglobal(L, "_currentScriptComponent");
                        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                        lua_pop(L, 1);
                        if (self && self->owner && self->owner->getParent() && self->owner->getParent()->getParent() &&
                            lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
                            auto grandparent = self->owner->getParent()->getParent();
                            grandparent->getTransform().setPosition(glm::vec3(
                                lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3)));
                        }
                        return 0;
                    });
                    lua_settable(L, -3);
                    
                    lua_pushstring(L, "getPosition");
                    lua_pushcfunction(L, [](lua_State* L) -> int {
                        lua_getglobal(L, "_currentScriptComponent");
                        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
                        lua_pop(L, 1);
                        if (self && self->owner && self->owner->getParent() && self->owner->getParent()->getParent()) {
                            auto grandparent = self->owner->getParent()->getParent();
                            glm::vec3 pos = grandparent->getTransform().getPosition();
                            lua_pushnumber(L, pos.x);
                            lua_pushnumber(L, pos.y);
                            lua_pushnumber(L, pos.z);
                            return 3;
                        }
                        lua_pushnil(L);
                        lua_pushnil(L);
                        lua_pushnil(L);
                        return 3;
                    });
                    lua_settable(L, -3);
                    
                    return 1;
                }
                lua_pushnil(L);
                return 1;
            });
            lua_settable(L, -3);
            
            return 1;
        }
        lua_pushnil(L);
        return 1;
    });
    lua_setglobal(luaState, "getParent");
}

void ScriptComponent::bindInputToLua() {
    if (!luaState) {
        return;
    }
    
    lua_newtable(luaState);
    
    // Action-based input functions
    lua_pushstring(luaState, "isActionPressed");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isActionPressed(actionName);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isActionHeld");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isActionHeld(actionName);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isActionReleased");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isActionReleased(actionName);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "getActionAxis");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        float result = inputManager.getActionAxis(actionName);
        lua_pushnumber(L, result);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "getActionVector2");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* actionName = luaL_checkstring(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 result = inputManager.getActionVector2(actionName);
        lua_pushnumber(L, result.x);
        lua_pushnumber(L, result.y);
        return 2;
    });
    lua_settable(luaState, -3);
    
    // Legacy direct input functions (for backward compatibility)
    lua_pushstring(luaState, "isKeyPressed");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
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
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isKeyDown");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
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
    lua_settable(luaState, -3);
    
    // Vita-specific input functions
#ifdef VITA_BUILD
    lua_pushstring(luaState, "isButtonPressed");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        int buttonCode = luaL_checkinteger(L, 1);
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isButtonPressed(buttonCode);
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "getLeftStick");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 stick = inputManager.getLeftStick();
        lua_pushnumber(L, stick.x);
        lua_pushnumber(L, stick.y);
        return 2;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "getRightStick");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 stick = inputManager.getRightStick();
        lua_pushnumber(L, stick.x);
        lua_pushnumber(L, stick.y);
        return 2;
    });
    lua_settable(luaState, -3);
#endif
    
#ifdef LINUX_BUILD
    // Mouse input functions (Linux only)
    lua_pushstring(luaState, "setMouseCapture");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        bool enabled = lua_toboolean(L, 1) != 0;
        auto& inputManager = GetEngine().getInputManager();
        inputManager.setMouseCapture(enabled);
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isMouseCaptured");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        bool result = inputManager.isMouseCaptured();
        lua_pushboolean(L, result);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "setDebugMouseInput");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        bool enabled = lua_toboolean(L, 1) != 0;
        auto& inputManager = GetEngine().getInputManager();
        inputManager.setDebugMouseInput(enabled);
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "getMousePosition");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 pos = inputManager.getMousePosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        return 2;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "getMouseDelta");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        glm::vec2 delta = inputManager.getMouseDelta();
        lua_pushnumber(L, delta.x);
        lua_pushnumber(L, delta.y);
        return 2;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isMouseButtonPressed");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
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
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isMouseButtonHeld");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
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
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isMouseButtonReleased");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
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
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "getMouseWheel");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto& inputManager = GetEngine().getInputManager();
        float wheel = inputManager.getMouseWheel();
        lua_pushnumber(L, wheel);
        return 1;
    });
    lua_settable(luaState, -3);
#endif
    
    lua_setglobal(luaState, "input");
}

void ScriptComponent::bindCameraToLua() {
    if (!luaState) {
        return;
    }
    
    lua_newtable(luaState);
    
    // Camera move function
    lua_pushstring(luaState, "move");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (self && self->owner && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
            float x = lua_tonumber(L, 1);
            float y = lua_tonumber(L, 2);
            float z = lua_tonumber(L, 3);
            
            glm::vec3 currentPos = self->owner->getTransform().getPosition();
            self->owner->getTransform().setPosition(currentPos + glm::vec3(x, y, z));
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    // Camera setPosition function
    lua_pushstring(luaState, "setPosition");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (self && self->owner && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
            float x = lua_tonumber(L, 1);
            float y = lua_tonumber(L, 2);
            float z = lua_tonumber(L, 3);
            self->owner->getTransform().setPosition(glm::vec3(x, y, z));
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    // Camera getPosition function
    lua_pushstring(luaState, "getPosition");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (self && self->owner) {
            glm::vec3 pos = self->owner->getTransform().getPosition();
            lua_pushnumber(L, pos.x);
            lua_pushnumber(L, pos.y);
            lua_pushnumber(L, pos.z);
            return 3;
        }
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 3;
    });
    lua_settable(luaState, -3);
    
    // Camera setRotation function
    lua_pushstring(luaState, "setRotation");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (self && self->owner && lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
            float x = lua_tonumber(L, 1);
            float y = lua_tonumber(L, 2);
            float z = lua_tonumber(L, 3);
            self->owner->getTransform().setEulerAngles(glm::vec3(x, y, z));
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    // Camera getRotation function
    lua_pushstring(luaState, "getRotation");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (self && self->owner) {
            glm::vec3 rot = self->owner->getTransform().getEulerAngles();
            lua_pushnumber(L, rot.x);
            lua_pushnumber(L, rot.y);
            lua_pushnumber(L, rot.z);
            return 3;
        }
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 3;
    });
    lua_settable(luaState, -3);
    
    // Camera setNodeLookAt function - makes camera look at a node by name
    lua_pushstring(luaState, "setNodeLookAt");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1); // Remove the userdata from stack
        
        if (!self || !self->owner) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        const char* targetNodeName = luaL_checkstring(L, 1);
        if (!targetNodeName) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        // Usage: camera.setNodeLookAt("NodeName") or camera.setNodeLookAt("NodeName", upX, upY, upZ)
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (lua_gettop(L) >= 4) {
            if (lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
                up.x = lua_tonumber(L, 2);
                up.y = lua_tonumber(L, 3);
                up.z = lua_tonumber(L, 4);
            }
        }
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene) {
                auto targetNode = activeScene->findNode(targetNodeName);
                if (targetNode) {
                    // Get the target node's world position
                    glm::mat4 worldMatrix = targetNode->getWorldMatrix();
                    glm::vec3 targetPosition = glm::vec3(worldMatrix[3]);
                    
                    // Make the camera look at the target position
                    self->owner->getTransform().lookAt(targetPosition, up);
                    
                    lua_pushboolean(L, true);
                    return 1;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "camera");
}

void ScriptComponent::bindPhysicsToLua() {
    if (!luaState) {
        return;
    }
    
    lua_newtable(luaState);
    
    lua_pushstring(luaState, "raycast");
    lua_pushcclosure(luaState, [](lua_State* L) -> int {
        float ox = luaL_checknumber(L, 1);
        float oy = luaL_checknumber(L, 2);
        float oz = luaL_checknumber(L, 3);
        float dx = luaL_checknumber(L, 4);
        float dy = luaL_checknumber(L, 5);
        float dz = luaL_checknumber(L, 6);
        float maxDist = luaL_checknumber(L, 7);
        int mask = (lua_gettop(L) >= 8 && lua_isnumber(L, 8)) ? (int)lua_tonumber(L, 8) : -1;
        glm::vec3 origin(ox, oy, oz);
        glm::vec3 dir(dx, dy, dz);
        float len = glm::length(dir);
        if (len < 1e-6f) { lua_pushnil(L); return 1; }
        dir /= len;
        glm::vec3 to = origin + dir * maxDist;
        bool hit = false;
        std::string hitNodeName;
        glm::vec3 hitPoint;
        glm::vec3 hitNormal;
        float hitDistance;
        bool ok = PhysicsManager::getInstance().raycastFromTo(origin, to, mask, hit, hitNodeName, hitPoint, hitNormal, hitDistance);
        if (!ok || !hit) { lua_pushnil(L); return 1; }
        lua_newtable(L);
        lua_pushstring(L, "hitNodeName");
        lua_pushstring(L, hitNodeName.c_str());
        lua_settable(L, -3);
        lua_pushstring(L, "hitPoint");
        lua_newtable(L);
        lua_pushstring(L, "x"); lua_pushnumber(L, hitPoint.x); lua_settable(L, -3);
        lua_pushstring(L, "y"); lua_pushnumber(L, hitPoint.y); lua_settable(L, -3);
        lua_pushstring(L, "z"); lua_pushnumber(L, hitPoint.z); lua_settable(L, -3);
        lua_settable(L, -3);
        lua_pushstring(L, "hitDistance");
        lua_pushnumber(L, hitDistance);
        lua_settable(L, -3);
        lua_pushstring(L, "hitNormal");
        lua_newtable(L);
        lua_pushstring(L, "x"); lua_pushnumber(L, hitNormal.x); lua_settable(L, -3);
        lua_pushstring(L, "y"); lua_pushnumber(L, hitNormal.y); lua_settable(L, -3);
        lua_pushstring(L, "z"); lua_pushnumber(L, hitNormal.z); lua_settable(L, -3);
        lua_settable(L, -3);
        return 1;
    }, 0);
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "addForce");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "Physics");
}

void ScriptComponent::bindNavToLua() {
    if (!luaState) return;
    lua_newtable(luaState);

    lua_pushstring(luaState, "find_path");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        float sx = luaL_checknumber(L, 1);
        float sy = luaL_checknumber(L, 2);
        float sz = luaL_checknumber(L, 3);
        float tx = luaL_checknumber(L, 4);
        float ty = luaL_checknumber(L, 5);
        float tz = luaL_checknumber(L, 6);
        bool allowDiagonal = true;
        NavGrid* grid = nullptr;
        if (lua_gettop(L) >= 7 && lua_istable(L, 7)) {
            lua_getfield(L, 7, "allow_diagonal");
            if (lua_isboolean(L, -1)) allowDiagonal = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            lua_getfield(L, 7, "volume");
            if (lua_type(L, -1) == LUA_TSTRING) {
                const char* volName = lua_tostring(L, -1);
                auto& engine = GetEngine();
                auto scene = engine.getSceneManager().getCurrentScene();
                if (scene && volName) {
                    auto node = scene->findNode(volName);
                    if (node) {
                        auto* vol = node->getComponent<NavVolumeComponent>();
                        if (vol) grid = vol->getGrid();
                    }
                }
            }
            lua_pop(L, 1);
        }
        if (!grid) grid = NavGridRegistry::get().getFirstGrid();
        glm::vec3 start(sx, sy, sz);
        glm::vec3 end(tx, ty, tz);
        std::vector<glm::vec3> path = grid ? grid->findPath(start, end, allowDiagonal) : std::vector<glm::vec3>();
        lua_newtable(L);
        for (size_t i = 0; i < path.size(); ++i) {
            lua_newtable(L);
            lua_pushnumber(L, path[i].x); lua_seti(L, -2, 1);
            lua_pushnumber(L, path[i].y); lua_seti(L, -2, 2);
            lua_pushnumber(L, path[i].z); lua_seti(L, -2, 3);
            lua_seti(L, -2, (lua_Integer)(i + 1));
        }
        return 1;
    });
    lua_settable(luaState, -3);

    lua_pushstring(luaState, "world_to_cell");
    lua_pushcclosure(luaState, [](lua_State* L) -> int {
        float wx = (float)luaL_checknumber(L, 1);
        float wz = (float)luaL_checknumber(L, 2);
        NavGrid* grid = NavGridRegistry::get().getFirstGrid();
        if (lua_gettop(L) >= 3 && lua_type(L, 3) == LUA_TSTRING) {
            const char* volName = lua_tostring(L, 3);
            auto& engine = GetEngine();
            auto scene = engine.getSceneManager().getCurrentScene();
            if (scene && volName) {
                auto node = scene->findNode(volName);
                if (node) {
                    auto* vol = node->getComponent<NavVolumeComponent>();
                    if (vol && vol->getGrid()) grid = vol->getGrid();
                }
            }
        }
        if (!grid) { lua_pushinteger(L, 0); lua_pushinteger(L, 0); return 2; }
        int ix = 0, iz = 0;
        grid->worldToCell(wx, wz, ix, iz);
        lua_pushinteger(L, ix);
        lua_pushinteger(L, iz);
        return 2;
    }, 0);
    lua_settable(luaState, -3);

    lua_pushstring(luaState, "get_agent");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene || !nodeName) { lua_pushnil(L); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushnil(L); return 1; }
        NavAgentComponent* agent = node->getComponent<NavAgentComponent>();
        if (!agent) { lua_pushnil(L); return 1; }
        lua_newtable(L);
        lua_pushstring(L, "_navAgent");
        lua_pushlightuserdata(L, agent);
        lua_settable(L, -3);
        lua_pushstring(L, "set_destination");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            if (!a || lua_gettop(LL) < 4) return 0;
            float x = luaL_checknumber(LL, 2);
            float y = luaL_checknumber(LL, 3);
            float z = luaL_checknumber(LL, 4);
            a->setDestination(glm::vec3(x, y, z));
            return 0;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "set_follow_path");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            if (!a || !lua_istable(LL, 2)) return 0;
            std::vector<glm::vec3> path;
            lua_len(LL, 2);
            lua_Integer len = lua_tointeger(LL, -1);
            lua_pop(LL, 1);
            for (lua_Integer i = 1; i <= len; ++i) {
                lua_geti(LL, 2, i);
                if (lua_istable(LL, -1)) {
                    lua_geti(LL, -1, 1);
                    lua_geti(LL, -2, 2);
                    lua_geti(LL, -3, 3);
                    float x = (float)lua_tonumber(LL, -3);
                    float y = (float)lua_tonumber(LL, -2);
                    float z = (float)lua_tonumber(LL, -1);
                    lua_pop(LL, 4);
                    path.push_back(glm::vec3(x, y, z));
                } else lua_pop(LL, 1);
            }
            a->setFollowPath(path);
            return 0;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "clear_destination");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            if (a) a->clearDestination();
            return 0;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "has_path");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            lua_pushboolean(LL, a ? a->hasPath() : false);
            return 1;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "path_count");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            lua_pushinteger(LL, a ? a->getPathSize() : 0);
            return 1;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "path_index");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            lua_pushinteger(LL, a ? a->getPathIndex() : 0);
            return 1;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "is_moving");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            lua_pushboolean(LL, a ? a->isMoving() : false);
            return 1;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "get_position");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            if (!a) { lua_pushnil(LL); return 1; }
            glm::vec3 p = a->getPosition();
            lua_pushnumber(LL, p.x);
            lua_pushnumber(LL, p.y);
            lua_pushnumber(LL, p.z);
            return 3;
        });
        lua_settable(L, -3);
        lua_pushstring(L, "on_path_finished");
        lua_pushcfunction(L, [](lua_State* LL) -> int {
            lua_getfield(LL, 1, "_navAgent");
            NavAgentComponent* a = static_cast<NavAgentComponent*>(lua_touserdata(LL, -1));
            lua_pop(LL, 1);
            if (!a) return 0;
            bool finished = a->consumePathFinishedFlag();
            lua_pushboolean(LL, finished ? 1 : 0);
            return 1;
        });
        lua_settable(L, -3);
        return 1;
    });
    lua_settable(luaState, -3);

    lua_setglobal(luaState, "Nav");
}

void ScriptComponent::bindRendererToLua() {
    if (!luaState) {
        return;
    }
    
    // Create renderer table
    lua_newtable(luaState);
    
    // Add renderer functions (simplified for now)
    lua_pushstring(luaState, "drawText");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "setText");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = lua_tostring(L, 1);
        const char* text = lua_tostring(L, 2);
        
        if (nodeName && text) {
#ifndef VITA_BUILD
            try {
#endif
                auto& engine = GetEngine();
                auto& sceneManager = engine.getSceneManager();
                auto currentScene = sceneManager.getCurrentScene();
                
                if (currentScene) {
                    auto node = currentScene->findNode(nodeName);
                    if (node) {
                        auto textComponent = node->getComponent<TextComponent>();
                        if (textComponent) {
                            textComponent->setText(text);
                        }
                    }
                }
#ifndef VITA_BUILD
            } catch (...) {
                // Handle exceptions gracefully
            }
#endif
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    // Add setTextRenderMode function to set text render mode (WORLD_SPACE=0, SCREEN_SPACE=1)
    lua_pushstring(luaState, "setTextRenderMode");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = lua_tostring(L, 1);
        int mode = luaL_checkinteger(L, 2);  // 0 = WORLD_SPACE, 1 = SCREEN_SPACE
        
        if (nodeName) {
#ifndef VITA_BUILD
            try {
#endif
                auto& engine = GetEngine();
                auto& sceneManager = engine.getSceneManager();
                auto currentScene = sceneManager.getCurrentScene();
                
                if (currentScene) {
                    auto node = currentScene->findNode(nodeName);
                    if (node) {
                        auto textComponent = node->getComponent<TextComponent>();
                        if (textComponent) {
                            textComponent->setRenderMode(static_cast<TextRenderMode>(mode));
                        }
                    }
                }
#ifndef VITA_BUILD
            } catch (...) {
                // Handle exceptions gracefully
            }
#endif
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "renderer");
}

void ScriptComponent::bindCommonFunctions() {
    if (!luaState) {
        return;
    }
    
    // Print function
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* message = lua_tostring(L, 1);
        if (message) {
#ifdef VITA_BUILD
            sceClibPrintf("Lua: %s\n", message);
#else
            std::cout << "Lua: " << message << std::endl;
#endif
        }
        return 0;
    });
    lua_setglobal(luaState, "print");
    
    // Platform detection global
#ifdef VITA_BUILD
    lua_pushboolean(luaState, true);
    lua_setglobal(luaState, "_VITA_BUILD");
#else
    lua_pushboolean(luaState, false);
    lua_setglobal(luaState, "_VITA_BUILD");
#endif
    
    // Time function
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
        lua_pushnumber(L, seconds);
        return 1;
    });
    lua_setglobal(luaState, "getTime");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        float fps = GetEngine().getTime().getFPS();
        lua_pushnumber(L, fps);
        return 1;
    });
    lua_setglobal(luaState, "getFPS");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        float dt = GetEngine().getTime().getDeltaTime();
        lua_pushnumber(L, dt);
        return 1;
    });
    lua_setglobal(luaState, "getDeltaTime");
    
    // Shared game pause state functions (accessible from all scripts)
    // Use a static variable that's shared across all Lua states
    // This allows scripts to check pause state without stopping scene updates
    static bool customGamePaused = false;
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_pushboolean(L, customGamePaused);
        return 1;
    });
    lua_setglobal(luaState, "isGamePaused");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        bool paused = lua_toboolean(L, 1) != 0;
        customGamePaused = paused;
        // Also set MenuManager pause state for physics/scene pausing
        // This pauses physics and other scene scripts, but the pause menu script
        // continues to run because it checks pause state before MenuManager pauses
        auto& menuManager = MenuManager::getInstance();
        if (paused) {
            // Create and show a hidden pause menu to pause the game
            menuManager.createMenu("pause_state_control", MenuType::PAUSE_MENU);
            menuManager.showMenu("pause_state_control");
        } else {
            // Hide the pause menu to resume
            menuManager.hideMenu("pause_state_control");
        }
        return 0;
    });
    lua_setglobal(luaState, "setGamePaused");
    
    // Quit game function
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        auto& engine = GetEngine();
        engine.setRunning(false);
        return 0;
    });
    lua_setglobal(luaState, "quitGame");
    
    // Generic camera position function - gets any camera's world position by name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* cameraName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && cameraName) {
                // Look for the specified camera node
                auto cameraNode = activeScene->getRootNode()->getChild(cameraName);
                if (cameraNode) {
                    // Get the world matrix to get the actual world position
                    auto worldMatrix = cameraNode->getWorldMatrix();
                    glm::vec3 worldPosition = glm::vec3(worldMatrix[3]);
                    
                    lua_pushnumber(L, worldPosition.x);
                    lua_pushnumber(L, worldPosition.y);
                    lua_pushnumber(L, worldPosition.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        // Fallback position if camera not found
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 2.0f);
        lua_pushnumber(L, 8.0f);
        return 3;
    });
    lua_setglobal(luaState, "getCameraPosition");
    
    // Generic node position function - gets any node's WORLD position by name (recursive search)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                // Use findNode to search recursively through the scene hierarchy
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    // Get the node's world matrix to get the actual world position
                    auto worldMatrix = node->getWorldMatrix();
                    glm::vec3 worldPosition = glm::vec3(worldMatrix[3]);
                    
                    lua_pushnumber(L, worldPosition.x);
                    lua_pushnumber(L, worldPosition.y);
                    lua_pushnumber(L, worldPosition.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        // Fallback position if node not found
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        return 3;
    });
    lua_setglobal(luaState, "getNodePosition");
    
    // Get parent node name for a node. Returns parent name or nil if no parent/not found.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        if (!nodeName) { lua_pushnil(L); return 1; }
#ifndef VITA_BUILD
        try {
#endif
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene) { lua_pushnil(L); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushnil(L); return 1; }
        auto* parent = node->getParent();
        if (!parent) { lua_pushnil(L); return 1; }
        lua_pushstring(L, parent->getName().c_str());
        return 1;
#ifndef VITA_BUILD
        } catch (...) {
            lua_pushnil(L);
            return 1;
        }
#endif
    });
    lua_setglobal(luaState, "getNodeParentName");
    
    // Generic node local position function - gets any node's LOCAL position by name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    // Get the node's LOCAL position (not world position)
                    glm::vec3 localPosition = node->getTransform().getPosition();
                    
                    lua_pushnumber(L, localPosition.x);
                    lua_pushnumber(L, localPosition.y);
                    lua_pushnumber(L, localPosition.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        // Fallback position if node not found
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        return 3;
    });
    lua_setglobal(luaState, "getNodeLocalPosition");
    
    // Generic node local position setter - sets any node's LOCAL position by name (for SCREEN_SPACE)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    // Set LOCAL position directly (no world-to-local conversion)
                    // This is what SCREEN_SPACE mode uses
                    node->getTransform().setPosition(glm::vec3(x, y, z));
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        return 0;
    });
    lua_setglobal(luaState, "setNodeLocalPosition");
    
    // Generic node lookAt function - makes any node look at a position or another node
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
        if (!nodeName) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        glm::vec3 targetPosition;
        bool hasPosition = false;
        
        // Check if second argument is a number (position) or string (node name)
        if (lua_gettop(L) >= 4 && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
            targetPosition.x = lua_tonumber(L, 2);
            targetPosition.y = lua_tonumber(L, 3);
            targetPosition.z = lua_tonumber(L, 4);
            hasPosition = true;
        } else if (lua_gettop(L) >= 2 && lua_isstring(L, 2)) {
            const char* targetNodeName = lua_tostring(L, 2);
            
#ifndef VITA_BUILD
            try {
#endif
                auto& engine = GetEngine();
                auto& sceneManager = engine.getSceneManager();
                auto activeScene = sceneManager.getCurrentScene();
                
                if (activeScene && targetNodeName) {
                    auto targetNode = activeScene->findNode(targetNodeName);
                    if (targetNode) {
                        glm::mat4 worldMatrix = targetNode->getWorldMatrix();
                        targetPosition = glm::vec3(worldMatrix[3]);
                        hasPosition = true;
                    }
                }
#ifndef VITA_BUILD
            } catch (...) {
                // Fallback if there's any error
            }
#endif
        }
        
        if (!hasPosition) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        int upVectorIndex = lua_isstring(L, 2) ? 3 : 5;
        if (lua_gettop(L) >= upVectorIndex + 2) {
            if (lua_isnumber(L, upVectorIndex) && lua_isnumber(L, upVectorIndex + 1) && lua_isnumber(L, upVectorIndex + 2)) {
                up.x = lua_tonumber(L, upVectorIndex);
                up.y = lua_tonumber(L, upVectorIndex + 1);
                up.z = lua_tonumber(L, upVectorIndex + 2);
            }
        }
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    // Convert target position to local space if node has a parent
                    glm::vec3 localTargetPosition = targetPosition;
                    auto parent = node->getParent();
                    if (parent) {
                        glm::mat4 parentWorldMatrix = parent->getWorldMatrix();
                        glm::mat4 parentWorldInv = glm::inverse(parentWorldMatrix);
                        glm::vec4 targetWorld4(targetPosition, 1.0f);
                        glm::vec4 targetLocal4 = parentWorldInv * targetWorld4;
                        localTargetPosition = glm::vec3(targetLocal4);
                    }
                    
                    node->getTransform().lookAt(localTargetPosition, up);
                    
                    lua_pushboolean(L, true);
                    return 1;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setNodeLookAt");
    
    // Generic node world position function - gets any node's world position by name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                // Look for the specified node
                auto node = activeScene->getRootNode()->getChild(nodeName);
                if (node) {
                    // Get the node's world matrix to get the actual world position
                    auto worldMatrix = node->getWorldMatrix();
                    glm::vec3 worldPosition = glm::vec3(worldMatrix[3]);
                    
                    lua_pushnumber(L, worldPosition.x);
                    lua_pushnumber(L, worldPosition.y);
                    lua_pushnumber(L, worldPosition.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        // Fallback position if node not found
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        return 3;
    });
    lua_setglobal(luaState, "getNodeWorldPosition");
    
    // Generic node position setter - sets any node's position by name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                // Use findNode to search recursively through the scene hierarchy (like getNodePosition)
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    // setNodePosition accepts WORLD coordinates and converts to local
                    // This matches how getNodePosition returns world coordinates
                    glm::vec3 worldPos(x, y, z);
                    
                    auto parent = node->getParent();
                    if (parent) {
                        // Convert world position to local position
                        glm::mat4 parentWorldMatrix = parent->getWorldMatrix();
                        glm::mat4 parentWorldInv = glm::inverse(parentWorldMatrix);
                        glm::vec4 worldPos4(worldPos, 1.0f);
                        glm::vec4 localPos4 = parentWorldInv * worldPos4;
                        glm::vec3 localPos = glm::vec3(localPos4);
                        
                        node->getTransform().setPosition(localPos);
                    } else {
                        // No parent - world position equals local position
                        node->getTransform().setPosition(worldPos);
                    }
                    
                    std::function<void(std::shared_ptr<SceneNode>)> updatePhysicsRecursive = [&](std::shared_ptr<SceneNode> n) {
                        auto physicsComp = n->getComponent<PhysicsComponent>();
                        if (physicsComp) {
                            physicsComp->forceUpdateCollisionShape();
                            physicsComp->syncTransformToPhysics();
                        }
                        for (size_t i = 0; i < n->getChildCount(); ++i) {
                            auto child = n->getChild(i);
                            if (child) {
                                updatePhysicsRecursive(child);
                            }
                        }
                    };
                    updatePhysicsRecursive(node);
                } else {
                    #ifdef _DEBUG
                    std::cout << "setNodePosition: Node '" << nodeName << "' not found!" << std::endl;
                    #endif
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodePosition");
    
    // Set velocity on a node's PhysicsComponent (for dynamic bodies)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        physicsComp->setLinearVelocity(glm::vec3(x, y, z));
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeVelocity");
    
    // Get velocity from a node's PhysicsComponent
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        glm::vec3 velocity = physicsComp->getLinearVelocity();
                        lua_pushnumber(L, velocity.x);
                        lua_pushnumber(L, velocity.y);
                        lua_pushnumber(L, velocity.z);
                        return 3;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 3;
    });
    lua_setglobal(luaState, "getNodeVelocity");
    
    // Raycast against dynamic rigidbodies. Returns nodeName, hitX, hitY, hitZ, hitDistance or nil.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        float ox = luaL_checknumber(L, 1);
        float oy = luaL_checknumber(L, 2);
        float oz = luaL_checknumber(L, 3);
        float dx = luaL_checknumber(L, 4);
        float dy = luaL_checknumber(L, 5);
        float dz = luaL_checknumber(L, 6);
        float maxDist = luaL_checknumber(L, 7);
        
#ifndef VITA_BUILD
        try {
#endif
            std::string hitNodeName;
            glm::vec3 hitPoint;
            float hitDistance;
            PhysicsManager::getInstance().raycast(
                glm::vec3(ox, oy, oz), glm::vec3(dx, dy, dz), maxDist,
                hitNodeName, hitPoint, hitDistance);
            if (hitNodeName.empty()) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushstring(L, hitNodeName.c_str());
            lua_pushnumber(L, hitPoint.x);
            lua_pushnumber(L, hitPoint.y);
            lua_pushnumber(L, hitPoint.z);
            lua_pushnumber(L, hitDistance);
            return 5;
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        lua_pushnil(L);
        return 1;
    });
    lua_setglobal(luaState, "physicsRaycast");
    
    // Raycast against all rigid bodies (static + dynamic); returns distance to closest obstacle or nil. Exclude node name (e.g. held object) so the ray doesn't hit it.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        float ox = luaL_checknumber(L, 1);
        float oy = luaL_checknumber(L, 2);
        float oz = luaL_checknumber(L, 3);
        float dx = luaL_checknumber(L, 4);
        float dy = luaL_checknumber(L, 5);
        float dz = luaL_checknumber(L, 6);
        float maxDist = luaL_checknumber(L, 7);
        const char* excludeName = lua_tostring(L, 8);
        std::string excludeNodeName = excludeName ? excludeName : "";
#ifndef VITA_BUILD
        try {
#endif
            float dist = PhysicsManager::getInstance().raycastClosestObstacle(
                glm::vec3(ox, oy, oz), glm::vec3(dx, dy, dz), maxDist, excludeNodeName);
            if (dist < 0.0f) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, dist);
            return 1;
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        lua_pushnil(L);
        return 1;
    });
    lua_setglobal(luaState, "physicsRaycastObstacle");

    // Raycast against all bodies (static + dynamic). Returns hitX, hitY, hitZ, normalX, normalY, normalZ, hitDistance
    lua_pushcfunction(luaState, ([](lua_State* L) -> int {
        float ox = luaL_checknumber(L, 1);
        float oy = luaL_checknumber(L, 2);
        float oz = luaL_checknumber(L, 3);
        float dx = luaL_checknumber(L, 4);
        float dy = luaL_checknumber(L, 5);
        float dz = luaL_checknumber(L, 6);
        float maxDist = luaL_checknumber(L, 7);
        const char* excludeName = lua_tostring(L, 8);
        const char* excludeName2 = lua_tostring(L, 9);
        std::string excludeNodeName = excludeName ? excludeName : "";
        std::string excludeNodeName2 = excludeName2 ? excludeName2 : "";
#ifndef VITA_BUILD
        try {
#endif
            glm::vec3 hitPoint, hitNormal;
            float hitDistance;
            bool hit = PhysicsManager::getInstance().raycastGround(
                glm::vec3(ox, oy, oz), glm::vec3(dx, dy, dz), maxDist, excludeNodeName, excludeNodeName2,
                hitPoint, hitNormal, hitDistance);
            if (!hit) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, hitPoint.x);
            lua_pushnumber(L, hitPoint.y);
            lua_pushnumber(L, hitPoint.z);
            lua_pushnumber(L, hitNormal.x);
            lua_pushnumber(L, hitNormal.y);
            lua_pushnumber(L, hitNormal.z);
            lua_pushnumber(L, hitDistance);
            return 7;
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        lua_pushnil(L);
        return 1;
    }));
    lua_setglobal(luaState, "physicsRaycastGround");

    // Raycast using a node's RaycastComponent. Returns hitNodeName, hitX, hitY, hitZ, normalX, normalY, normalZ, hitDistance or nil.
    lua_pushcfunction(luaState, ([](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene) {
            lua_pushnil(L);
            return 1;
        }
        auto node = activeScene->findNode(nodeName);
        if (!node) {
            lua_pushnil(L);
            return 1;
        }
        auto* raycastComp = node->getComponent<RaycastComponent>();
        if (!raycastComp) {
            lua_pushnil(L);
            return 1;
        }
        bool hit = false;
        std::string hitNodeName;
        glm::vec3 hitPoint, hitNormal;
        float hitDistance;
        bool ok = raycastComp->performRaycast(hit, hitNodeName, hitPoint, hitNormal, hitDistance);
        if (!ok || !hit) {
            lua_pushnil(L);
            return 1;
        }
        lua_pushstring(L, hitNodeName.c_str());
        lua_pushnumber(L, hitPoint.x);
        lua_pushnumber(L, hitPoint.y);
        lua_pushnumber(L, hitPoint.z);
        lua_pushnumber(L, hitNormal.x);
        lua_pushnumber(L, hitNormal.y);
        lua_pushnumber(L, hitNormal.z);
        lua_pushnumber(L, hitDistance);
        return 8;
    }));
    lua_setglobal(luaState, "raycastFromNode");

    // Get world-space ray from/to for a node with RaycastComponent. Returns fromX, fromY, fromZ, toX, toY, toZ.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene || !nodeName) {
            lua_pushnil(L);
            return 1;
        }
        auto node = activeScene->findNode(nodeName);
        if (!node) {
            lua_pushnil(L);
            return 1;
        }
        auto* raycastComp = node->getComponent<RaycastComponent>();
        if (!raycastComp) {
            lua_pushnil(L);
            return 1;
        }
        glm::vec3 wFrom = raycastComp->getWorldFrom();
        glm::vec3 wTo = raycastComp->getWorldTo();
        lua_pushnumber(L, wFrom.x);
        lua_pushnumber(L, wFrom.y);
        lua_pushnumber(L, wFrom.z);
        lua_pushnumber(L, wTo.x);
        lua_pushnumber(L, wTo.y);
        lua_pushnumber(L, wTo.z);
        return 6;
    });
    lua_setglobal(luaState, "getRaycastWorldRay");

    // Raycast in world space from (fromX,fromY,fromZ) to (toX,toY,toZ). Optional 7th arg: excludeNodeName.
    lua_pushcfunction(luaState, ([](lua_State* L) -> int {
        float fromX = luaL_checknumber(L, 1);
        float fromY = luaL_checknumber(L, 2);
        float fromZ = luaL_checknumber(L, 3);
        float toX = luaL_checknumber(L, 4);
        float toY = luaL_checknumber(L, 5);
        float toZ = luaL_checknumber(L, 6);
        glm::vec3 from(fromX, fromY, fromZ);
        glm::vec3 to(toX, toY, toZ);
        SceneNode* excludeNode = nullptr;
        if (lua_gettop(L) >= 7 && lua_type(L, 7) == LUA_TSTRING) {
            const char* excludeName = lua_tostring(L, 7);
            auto& engine = GetEngine();
            auto scene = engine.getSceneManager().getCurrentScene();
            if (scene && excludeName) {
                auto node = scene->findNode(excludeName);
                if (node) excludeNode = node.get();
            }
        }
        bool hit = false;
        std::string hitNodeName;
        glm::vec3 hitPoint, hitNormal;
        float hitDistance;
        bool ok = PhysicsManager::getInstance().raycastFromTo(from, to, -1, hit, hitNodeName, hitPoint, hitNormal, hitDistance, excludeNode);
        if (!ok || !hit) {
            lua_pushnil(L);
            return 1;
        }
        lua_pushstring(L, hitNodeName.c_str());
        lua_pushnumber(L, hitPoint.x);
        lua_pushnumber(L, hitPoint.y);
        lua_pushnumber(L, hitPoint.z);
        lua_pushnumber(L, hitNormal.x);
        lua_pushnumber(L, hitNormal.y);
        lua_pushnumber(L, hitNormal.z);
        lua_pushnumber(L, hitDistance);
        return 8;
    }));
    lua_setglobal(luaState, "raycastFromToWorld");

    // Set the debug ray drawn when physics debug draw is enabled.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        float fromX = luaL_checknumber(L, 1);
        float fromY = luaL_checknumber(L, 2);
        float fromZ = luaL_checknumber(L, 3);
        float toX = luaL_checknumber(L, 4);
        float toY = luaL_checknumber(L, 5);
        float toZ = luaL_checknumber(L, 6);
        glm::vec3 from(fromX, fromY, fromZ);
        glm::vec3 to(toX, toY, toZ);
        PhysicsManager::getInstance().setDebugRay(from, to);
        return 0;
    });
    lua_setglobal(luaState, "setDebugRayFromTo");

    // Set angular velocity on a node's PhysicsComponent.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        physicsComp->setAngularVelocity(glm::vec3(x, y, z));
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeAngularVelocity");
    
    // Set gravity enabled/disabled on a node's PhysicsComponent
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        bool enabled = lua_toboolean(L, 2) != 0;
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        physicsComp->setGravityEnabled(enabled);
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeGravityEnabled");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* childName = luaL_checkstring(L, 1);
        bool enabled = lua_toboolean(L, 2) != 0;
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        if (!self || !self->owner || !childName) return 0;
        for (size_t i = 0; i < self->owner->getChildCount(); ++i) {
            auto child = self->owner->getChild(i);
            if (child && child->getName() == childName) {
                auto* phys = child->getComponent<PhysicsComponent>();
                if (phys) {
                    phys->setGravityEnabled(enabled);
                    lua_pushboolean(L, true);
                    return 1;
                }
                break;
            }
        }
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setOwnerChildGravityEnabled");
    
    // Set gravity on the current script owner's FIRST child that has PhysicsComponent
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        bool enabled = lua_toboolean(L, 1) != 0;
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        if (!self || !self->owner) { lua_pushboolean(L, false); return 1; }
        for (size_t i = 0; i < self->owner->getChildCount(); ++i) {
            auto child = self->owner->getChild(i);
            if (child) {
                auto* phys = child->getComponent<PhysicsComponent>();
                if (phys) {
                    phys->setGravityEnabled(enabled);
                    lua_pushboolean(L, true);
                    return 1;
                }
            }
        }
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setOwnerFirstPhysicsChildGravityEnabled");
    
    // Set angular factor on the current script owner's FIRST child that has PhysicsComponent.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        float x = luaL_checknumber(L, 1);
        float y = luaL_checknumber(L, 2);
        float z = luaL_checknumber(L, 3);
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        if (!self || !self->owner) return 0;
        for (size_t i = 0; i < self->owner->getChildCount(); ++i) {
            auto child = self->owner->getChild(i);
            if (child) {
                auto* phys = child->getComponent<PhysicsComponent>();
                if (phys) {
                    phys->setAngularFactor(glm::vec3(x, y, z));
                    break;
                }
            }
        }
        return 0;
    });
    lua_setglobal(luaState, "setOwnerFirstPhysicsChildAngularFactor");
    
    // Set angular factor on the current script owner's child by name.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* childName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        if (!self || !self->owner || !childName) return 0;
        for (size_t i = 0; i < self->owner->getChildCount(); ++i) {
            auto child = self->owner->getChild(i);
            if (child && child->getName() == childName) {
                auto* phys = child->getComponent<PhysicsComponent>();
                if (phys) {
                    phys->setAngularFactor(glm::vec3(x, y, z));
                    lua_pushboolean(L, true);
                    return 1;
                }
                break;
            }
        }
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setOwnerChildAngularFactor");
    
    // Set physics body type: "static", "dynamic", or "kinematic". Returns true if node had PhysicsComponent and type was set.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* bodyTypeStr = luaL_checkstring(L, 2);
        if (!nodeName || !bodyTypeStr) { lua_pushboolean(L, false); return 1; }
#ifndef VITA_BUILD
        try {
#endif
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene) { lua_pushboolean(L, false); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushboolean(L, false); return 1; }
        auto* physicsComp = node->getComponent<PhysicsComponent>();
        if (!physicsComp) { lua_pushboolean(L, false); return 1; }
        std::string bt(bodyTypeStr);
        PhysicsBodyType bodyType = PhysicsBodyType::STATIC;
        if (bt == "dynamic") bodyType = PhysicsBodyType::DYNAMIC;
        else if (bt == "kinematic") bodyType = PhysicsBodyType::KINEMATIC;
        physicsComp->setBodyType(bodyType);
        lua_pushboolean(L, true);
        return 1;
#ifndef VITA_BUILD
        } catch (...) {
            lua_pushboolean(L, false);
            return 1;
        }
#endif
    });
    lua_setglobal(luaState, "setNodeBodyType");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        int enabled = lua_toboolean(L, 1);
        PhysicsManager::getInstance().setDebugDrawEnabled(enabled != 0);
        return 0;
    });
    lua_setglobal(luaState, "setPhysicsDebugDrawEnabled");
    
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_pushboolean(L, PhysicsManager::getInstance().isDebugDrawEnabled() ? 1 : 0);
        return 1;
    });
    lua_setglobal(luaState, "getPhysicsDebugDrawEnabled");
    
    // Push node's current world transform to its PhysicsComponent.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        physicsComp->syncTransformToPhysics();
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        return 0;
    });
    lua_setglobal(luaState, "syncNodeTransformToPhysics");
    
    // Get node world rotation as euler angles (degrees). Returns pitch, yaw, roll.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::mat4 worldMatrix = node->getWorldMatrix();
                    glm::quat worldQuat = glm::quat_cast(worldMatrix);
                    glm::vec3 eulerRad = glm::eulerAngles(worldQuat);
                    glm::vec3 eulerDeg = glm::degrees(eulerRad);
                    lua_pushnumber(L, eulerDeg.x);
                    lua_pushnumber(L, eulerDeg.y);
                    lua_pushnumber(L, eulerDeg.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 3;
    });
    lua_setglobal(luaState, "getNodeWorldEuler");
    
    // Set node world rotation from euler angles (degrees).
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float ex = luaL_checknumber(L, 2);
        float ey = luaL_checknumber(L, 3);
        float ez = luaL_checknumber(L, 4);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::quat worldQuat = glm::quat(glm::radians(glm::vec3(ex, ey, ez)));
                    auto parent = node->getParent();
                    if (parent) {
                        glm::quat parentRot = glm::quat_cast(parent->getWorldMatrix());
                        glm::quat localQuat = glm::inverse(parentRot) * worldQuat;
                        node->getTransform().setRotation(localQuat);
                    } else {
                        node->getTransform().setRotation(worldQuat);
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        return 0;
    });
    lua_setglobal(luaState, "setNodeWorldRotation");
    
    // Set node world rotation so its -Z (forward) points at world position (tx, ty, tz). Node position unchanged.
    // Named setNodeLookAtPosition to avoid overwriting the global setNodeLookAt
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float tx = luaL_checknumber(L, 2);
        float ty = luaL_checknumber(L, 3);
        float tz = luaL_checknumber(L, 4);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::vec3 pos = glm::vec3(node->getWorldMatrix()[3]);
                    glm::vec3 forward = glm::normalize(glm::vec3(tx, ty, tz) - pos);
                    float len = glm::length(forward);
                    if (len > 1e-6f) {
                        glm::vec3 up(0.0f, 1.0f, 0.0f);
                        if (std::abs(glm::dot(forward, up)) > 0.99f)
                            up = glm::vec3(0.0f, 0.0f, 1.0f);
                        glm::vec3 right = glm::normalize(glm::cross(up, forward));
                        glm::vec3 actualUp = glm::cross(forward, right);
                        glm::mat3 rot(right, actualUp, -forward);
                        glm::quat worldQuat = glm::quat_cast(rot);
                        auto parent = node->getParent();
                        if (parent) {
                            glm::quat parentRot = glm::quat_cast(parent->getWorldMatrix());
                            glm::quat localQuat = glm::inverse(parentRot) * worldQuat;
                            node->getTransform().setRotation(localQuat);
                        } else {
                            node->getTransform().setRotation(worldQuat);
                        }
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        return 0;
    });
    lua_setglobal(luaState, "setNodeLookAtPosition");
    
    // Apply rotation around world axis (degrees) to node's current world rotation.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float ax = luaL_checknumber(L, 2);
        float ay = luaL_checknumber(L, 3);
        float az = luaL_checknumber(L, 4);
        float angleDeg = luaL_checknumber(L, 5);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::vec3 axis(ax, ay, az);
                    float len = glm::length(axis);
                    if (len > 0.0001f) {
                        axis /= len;
                        glm::quat worldQuat = glm::quat_cast(node->getWorldMatrix());
                        glm::quat deltaQuat = glm::angleAxis(glm::radians(angleDeg), axis);
                        glm::quat newWorldQuat = deltaQuat * worldQuat;
                        auto parent = node->getParent();
                        if (parent) {
                            glm::quat parentRot = glm::quat_cast(parent->getWorldMatrix());
                            glm::quat localQuat = glm::inverse(parentRot) * newWorldQuat;
                            node->getTransform().setRotation(localQuat);
                        } else {
                            node->getTransform().setRotation(newWorldQuat);
                        }
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        return 0;
    });
    lua_setglobal(luaState, "applyNodeRotationAroundAxis");
    
    // Set angular factor on a node's PhysicsComponent.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        physicsComp->setAngularFactor(glm::vec3(x, y, z));
                        if (x == 0.0f && y == 0.0f && z == 0.0f) {
                            physicsComp->setAngularVelocity(glm::vec3(0, 0, 0));
                        }
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeAngularFactor");
    
    // Get angular factor from a node's PhysicsComponent
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        glm::vec3 factor = physicsComp->getAngularFactor();
                        lua_pushnumber(L, factor.x);
                        lua_pushnumber(L, factor.y);
                        lua_pushnumber(L, factor.z);
                        return 3;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        lua_pushnumber(L, 1);
        lua_pushnumber(L, 1);
        lua_pushnumber(L, 1);
        return 3;
    });
    lua_setglobal(luaState, "getNodeAngularFactor");
    
    // Generic node rotation setter - sets any node's rotation by name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                // Use findNode to search recursively through the scene hierarchy
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    // Set rotation as Euler angles
                    node->getTransform().setEulerAngles(glm::vec3(x, y, z));
                } else {
                    #ifdef _DEBUG
                    std::cout << "setNodeRotation: Node '" << nodeName << "' not found!" << std::endl;
                    #endif
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeRotation");
    
    // Generic node rotation getter - gets any node's rotation by name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::vec3 rot = node->getTransform().getEulerAngles();
                    lua_pushnumber(L, rot.x);
                    lua_pushnumber(L, rot.y);
                    lua_pushnumber(L, rot.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 3;
    });
    lua_setglobal(luaState, "getNodeRotation");
    
    // Get a node's world-space forward direction (normalized). Returns fx, fy, fz.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::mat4 worldMatrix = node->getWorldMatrix();
                    glm::vec3 forward = -glm::normalize(glm::vec3(worldMatrix[2]));
                    lua_pushnumber(L, forward.x);
                    lua_pushnumber(L, forward.y);
                    lua_pushnumber(L, forward.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, -1);
        return 3;
    });
    lua_setglobal(luaState, "getNodeForward");
    
    // Get a node's world-space right direction (normalized). Returns rx, ry, rz.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::mat4 worldMatrix = node->getWorldMatrix();
                    glm::vec3 right = glm::normalize(glm::vec3(worldMatrix[0]));
                    lua_pushnumber(L, right.x); lua_pushnumber(L, right.y); lua_pushnumber(L, right.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        lua_pushnumber(L, 1); lua_pushnumber(L, 0); lua_pushnumber(L, 0);
        return 3;
    });
    lua_setglobal(luaState, "getNodeRight");
    
    // Get a node's world-space up direction (normalized). Returns ux, uy, uz.
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    glm::mat4 worldMatrix = node->getWorldMatrix();
                    glm::vec3 up = glm::normalize(glm::vec3(worldMatrix[1]));
                    lua_pushnumber(L, up.x); lua_pushnumber(L, up.y); lua_pushnumber(L, up.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        lua_pushnumber(L, 0); lua_pushnumber(L, 1); lua_pushnumber(L, 0);
        return 3;
    });
    lua_setglobal(luaState, "getNodeUp");
    
    // Generic node scale setter - sets any node's scale by name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float z = luaL_checknumber(L, 4);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                // Use findNode to search recursively through the scene hierarchy
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    // Set scale
                    node->getTransform().setScale(glm::vec3(x, y, z));
                } else {
                    #ifdef _DEBUG
                    std::cout << "setNodeScale: Node '" << nodeName << "' not found!" << std::endl;
                    #endif
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeScale");
    
    // Call a script function on a node's ScriptComponent
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* functionName = luaL_checkstring(L, 2);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName && functionName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto scriptComp = node->getComponent<ScriptComponent>();
                    if (scriptComp) {
                        scriptComp->callScriptFunction(functionName);
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "callNodeScriptFunction");
    
    // Call a script function on a node's ScriptComponent with a float parameter
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* functionName = luaL_checkstring(L, 2);
        float param = luaL_checknumber(L, 3);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName && functionName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto scriptComp = node->getComponent<ScriptComponent>();
                    if (scriptComp) {
                        scriptComp->callScriptFunction(functionName, param);
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "callNodeScriptFunctionWithParam");
    
    // Set node visibility function - shows/hides nodes (stops rendering)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        bool visible = lua_toboolean(L, 2) != 0;
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    node->setVisible(visible);
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeVisible");
    
    // setJointEnabled(jointNodeName, enabled) - enable/disable JointComponent on a node (e.g. PlayerJoint)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        bool enabled = lua_toboolean(L, 2) != 0;
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto jointComp = node->getComponent<JointComponent>();
                    if (jointComp) {
                        jointComp->setEnabled(enabled);
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setJointEnabled");
    
    // setBeamEndpoints(nodeName, startX, startY, startZ, endX, endY, endZ) - for BeamRenderer (Garry's Mod-style beam)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float sx = luaL_checknumber(L, 2);
        float sy = luaL_checknumber(L, 3);
        float sz = luaL_checknumber(L, 4);
        float ex = luaL_checknumber(L, 5);
        float ey = luaL_checknumber(L, 6);
        float ez = luaL_checknumber(L, 7);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    BeamRenderer* beam = node->getComponent<BeamRenderer>();
                    if (beam) {
                        beam->setBeamStart(glm::vec3(sx, sy, sz));
                        beam->setBeamEnd(glm::vec3(ex, ey, ez));
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        return 0;
    });
    lua_setglobal(luaState, "setBeamEndpoints");
    
    // setBeamWidth(nodeName, width)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float width = luaL_checknumber(L, 2);
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    BeamRenderer* beam = node->getComponent<BeamRenderer>();
                    if (beam) {
                        beam->setBeamWidth(width);
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {}
#endif
        return 0;
    });
    lua_setglobal(luaState, "setBeamWidth");
    
    // Get node visibility function - checks if a node is visible
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    lua_pushboolean(L, node->isVisible());
                    return 1;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "isNodeVisible");
    
    // Set node active state function - enables/disables node updates
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        bool active = lua_toboolean(L, 2) != 0;
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    node->setActive(active);
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        return 0;
    });
    lua_setglobal(luaState, "setNodeActive");
    
    // Get node active state function - checks if a node is active
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    lua_pushboolean(L, node->isActive());
                    return 1;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "isNodeActive");

    // setCameraViewport(nodeName, x, y, width, height)
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float x = luaL_checknumber(L, 2);
        float y = luaL_checknumber(L, 3);
        float w = luaL_checknumber(L, 4);
        float h = luaL_checknumber(L, 5);

#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();

            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto cameraComp = node->getComponent<CameraComponent>();
                    if (cameraComp) {
                        auto clamp01 = [](float v) {
                            if (v < 0.0f) return 0.0f;
                            if (v > 1.0f) return 1.0f;
                            return v;
                        };
                        x = clamp01(x);
                        y = clamp01(y);
                        w = clamp01(w);
                        h = clamp01(h);
                        cameraComp->setViewport(x, y, w, h);
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif

        return 0;
    });
    lua_setglobal(luaState, "setCameraViewport");

    // getCameraViewport(nodeName) -> x, y, width, height
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);

#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();

            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto cameraComp = node->getComponent<CameraComponent>();
                    if (cameraComp) {
                        glm::vec4 vp = cameraComp->getViewport();
                        lua_pushnumber(L, vp.x);
                        lua_pushnumber(L, vp.y);
                        lua_pushnumber(L, vp.z);
                        lua_pushnumber(L, vp.w);
                        return 4;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        //Default values
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 1.0f);
        lua_pushnumber(L, 1.0f);
        return 4;
    });
    lua_setglobal(luaState, "getCameraViewport");

    lua_pushcfunction(luaState, [](lua_State* L) -> int {

        const char* nodeName = luaL_checkstring(L, 1);
        bool active = lua_toboolean(L, 2);
    
    #ifndef VITA_BUILD
        try {
    #endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto scene = sceneManager.getCurrentScene();
    
            if (scene && nodeName) {
    
                auto node = scene->findNode(nodeName);
    
                if (node) {
    
                    auto camera = node->getComponent<CameraComponent>();
    
                    if (camera) {
                        camera->setActive(active);
                    }
                }
            }
    
    #ifndef VITA_BUILD
        } catch (...) {}
    #endif
    
        return 0;
    });
    lua_setglobal(luaState, "setCameraActive");

    // Get active camera position function
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene) {
                auto activeCamera = activeScene->getActiveCamera();
                if (activeCamera) {
                    auto worldMatrix = activeCamera->getWorldMatrix();
                    glm::vec3 worldPosition = glm::vec3(worldMatrix[3]);
                    
                    lua_pushnumber(L, worldPosition.x);
                    lua_pushnumber(L, worldPosition.y);
                    lua_pushnumber(L, worldPosition.z);
                    return 3;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Fallback if there's any error
        }
#endif
        
        // Fallback position if active camera not found
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 2.0f);
        lua_pushnumber(L, 8.0f);
        return 3;
    });
    lua_setglobal(luaState, "getActiveCameraPosition");
}

void ScriptComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Status: %s", scriptLoaded ? (scriptStarted ? "Running" : "Loaded") : "Not Loaded");
        
        // Script path - use a unique buffer per component instance
        if (scriptPathBuffers.find(this) == scriptPathBuffers.end()) {
            scriptPathBuffers[this] = scriptPath;
        }
        scriptPathBuffers[this] = scriptPath; // Update with current path
        
        char scriptPathBuffer[256];
        strncpy(scriptPathBuffer, scriptPathBuffers[this].c_str(), sizeof(scriptPathBuffer) - 1);
        scriptPathBuffer[sizeof(scriptPathBuffer) - 1] = '\0';
        
        if (ImGui::InputText("Script Path", scriptPathBuffer, sizeof(scriptPathBuffer))) {
            scriptPathBuffers[this] = std::string(scriptPathBuffer);
            setScriptPath(scriptPathBuffers[this]); // Save immediately as you type
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Load Script")) {
            if (!scriptPath.empty()) {
                loadScript(scriptPath);
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Reload Script")) {
            reloadScript();
        }
        
        // Script properties
        if (scriptLoaded) {
            ImGui::Separator();
            ImGui::Text("Script Properties:");
            
            // Show available functions
            ImGui::Text("Available Functions:");
            if (hasScriptFunction("start")) ImGui::BulletText("start()");
            if (hasScriptFunction("update")) ImGui::BulletText("update(deltaTime)");
            if (hasScriptFunction("fixedUpdate")) ImGui::BulletText("fixedUpdate(deltaTime) [60 Hz, before physics step]");
            if (hasScriptFunction("lateUpdate")) ImGui::BulletText("lateUpdate(deltaTime) [after physics]");
            if (hasScriptFunction("render")) ImGui::BulletText("render()");
            if (hasScriptFunction("destroy")) ImGui::BulletText("destroy()");
        }

        ImGui::Separator();
        ImGui::Checkbox("Pause Exempt", &pauseExempt);
    }
#endif
}

void ScriptComponent::bindPickupZoneToLua() {
    if (!luaState) {
        return;
    }
    
    // Binding pickup zone system to individual Lua state
    
    // Create pickup zone table
    lua_newtable(luaState);
    
    // Global pickup zone state
    lua_pushstring(luaState, "zones");
    lua_newtable(luaState);
    lua_settable(luaState, -3);
    
    // Create pickup zone function
    lua_pushstring(luaState, "createZone");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        if (lua_gettop(L) != 7) {
            lua_pushstring(L, "createZone requires 7 arguments: name, x, y, z, width, height, depth");
            lua_error(L);
            return 0;
        }
        
        const char* name = lua_tostring(L, 1);
        double x = lua_tonumber(L, 2);
        double y = lua_tonumber(L, 3);
        double z = lua_tonumber(L, 4);
        double width = lua_tonumber(L, 5);
        double height = lua_tonumber(L, 6);
        double depth = lua_tonumber(L, 7);
        
        // Create zone table
        lua_newtable(L);
        
        // Set properties
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
        
        // Store in global zones table
        lua_getglobal(L, "pickupZone");
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "zones");
            lua_gettable(L, -2);
            if (lua_istable(L, -1)) {
                lua_pushstring(L, name);
                lua_pushvalue(L, -4); // Copy the zone table
                lua_settable(L, -3);
            }
            lua_pop(L, 1); // Pop zones table
        }
        lua_pop(L, 1); // Pop pickupZone table
        
        return 1; // Return the zone table
    });
    lua_settable(luaState, -3);
    
    // Check if object is in zone function
    lua_pushstring(luaState, "isObjectInZone");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        if (lua_gettop(L) != 2) {
            lua_pushstring(L, "isObjectInZone requires 2 arguments: zoneName, objectName");
            lua_error(L);
            return 0;
        }
        
        const char* zoneName = lua_tostring(L, 1);
        const char* objectName = lua_tostring(L, 2);
        
        // Get zones table
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "objects");
            lua_gettable(L, -2);
            if (lua_istable(L, -1)) {
                lua_pushstring(L, objectName);
                lua_gettable(L, -2);
                bool exists = !lua_isnil(L, -1);
                lua_pop(L, 1);
                lua_pushboolean(L, exists);
                lua_pop(L, 3); // Clean up stack
                return 1;
            }
        }
        
        lua_pushboolean(L, false);
        lua_pop(L, 2); // Clean up stack
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Add object to zone function
    lua_pushstring(luaState, "addObjectToZone");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        if (lua_gettop(L) != 2) {
            lua_pushstring(L, "addObjectToZone requires 2 arguments: zoneName, objectName");
            lua_error(L);
            return 0;
        }
        
        const char* zoneName = lua_tostring(L, 1);
        const char* objectName = lua_tostring(L, 2);
        
        // Get zones table
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "objects");
            lua_gettable(L, -2);
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushstring(L, "objects");
                lua_pushvalue(L, -2);
                lua_settable(L, -4);
            }
            lua_pushstring(L, objectName);
            lua_pushboolean(L, true);
            lua_settable(L, -3);
            lua_pop(L, 2); // Clean up stack
        }
        
        lua_pop(L, 2); // Clean up stack
        return 0;
    });
    lua_settable(luaState, -3);
    
    // Remove object from zone function
    lua_pushstring(luaState, "removeObjectFromZone");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        if (lua_gettop(L) != 2) {
            lua_pushstring(L, "removeObjectFromZone requires 2 arguments: zoneName, objectName");
            lua_error(L);
            return 0;
        }
        
        const char* zoneName = lua_tostring(L, 1);
        const char* objectName = lua_tostring(L, 2);
        
        // Get zones table
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "objects");
            lua_gettable(L, -2);
            if (lua_istable(L, -1)) {
                lua_pushstring(L, objectName);
                lua_pushnil(L);
                lua_settable(L, -3);
            }
            lua_pop(L, 1);
        }
        
        lua_pop(L, 2); // Clean up stack
        return 0;
    });
    lua_settable(luaState, -3);
    
    // Get objects in zone function
    lua_pushstring(luaState, "getObjectsInZone");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        if (lua_gettop(L) != 1) {
            lua_pushstring(L, "getObjectsInZone requires 1 argument: zoneName");
            lua_error(L);
            return 0;
        }
        
        const char* zoneName = lua_tostring(L, 1);
        
        // Get zones table
        lua_getglobal(L, "pickupZone");
        lua_pushstring(L, "zones");
        lua_gettable(L, -2);
        lua_pushstring(L, zoneName);
        lua_gettable(L, -2);
        
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "objects");
            lua_gettable(L, -2);
            if (lua_istable(L, -1)) {
                lua_pop(L, 2); // Clean up stack, keep objects table
                return 1; // Return objects table
            }
        }
        
        lua_newtable(L); // Return empty table
        lua_pop(L, 2); // Clean up stack
        return 1; // Return objects table
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "pickupZone");
    
    // Pickup zone system bound successfully
}

void ScriptComponent::bindArea3DToLua() {
    if (!luaState) {
        return;
    }
    
    // Create area3D table
    lua_newtable(luaState);
    
    // Function to get Area3D component from a node by name
    lua_pushstring(luaState, "getComponent");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (!activeScene) {
#ifdef VITA_BUILD
                printf("Area3D.getComponent: No active scene\n");
#else
                std::cout << "Area3D.getComponent: No active scene" << std::endl;
#endif
                lua_pushnil(L);
                return 1;
            }
            
            if (!nodeName) {
#ifdef VITA_BUILD
                printf("Area3D.getComponent: nodeName is null\n");
#else
                std::cout << "Area3D.getComponent: nodeName is null" << std::endl;
#endif
                lua_pushnil(L);
                return 1;
            }
            
            auto node = activeScene->findNode(nodeName);
            if (!node) {
#ifdef VITA_BUILD
                printf("Area3D.getComponent: Node '%s' not found in scene\n", nodeName);
#else
                std::cout << "Area3D.getComponent: Node '" << nodeName << "' not found in scene" << std::endl;
#endif
                lua_pushnil(L);
                return 1;
            }
            
            // Debug: Check node hierarchy
            auto parent = node->getParent();
            std::string parentName = parent ? parent->getName() : "null";
#ifdef VITA_BUILD
            printf("Area3D.getComponent: Node '%s' found, parent='%s'\n", nodeName, parentName.c_str());
#else
            std::cout << "Area3D.getComponent: Node '" << nodeName << "' found, parent='" << parentName << "'" << std::endl;
#endif
            
            // Debug: Check if node has any components
            const auto& allComponents = node->getAllComponents();
#ifdef VITA_BUILD
            printf("Area3D.getComponent: Node '%s' has %zu component(s)\n", nodeName, allComponents.size());
#else
            std::cout << "Area3D.getComponent: Node '" << nodeName << "' has " << allComponents.size() << " component(s)" << std::endl;
#endif
            for (size_t i = 0; i < allComponents.size(); ++i) {
                const auto& comp = allComponents[i];
                if (comp) {
#ifdef VITA_BUILD
                    printf("  Component %zu: type='%s', enabled=%d\n", i, comp->getTypeName().c_str(), comp->isEnabled());
#else
                    std::cout << "  Component " << i << ": type='" << comp->getTypeName() << "', enabled=" << comp->isEnabled() << std::endl;
#endif
                }
            }
            
            // Check if Area3DComponent exists by type name
            bool hasArea3DByName = node->hasComponent("Area3DComponent");
#ifdef VITA_BUILD
            printf("Area3D.getComponent: hasComponent('Area3DComponent') = %d\n", hasArea3DByName);
#else
            std::cout << "Area3D.getComponent: hasComponent('Area3DComponent') = " << hasArea3DByName << std::endl;
#endif
            
            auto area3DComp = node->getComponent<Area3DComponent>();
            if (!area3DComp) {
#ifdef VITA_BUILD
                printf("Area3D.getComponent: Node '%s' found but getComponent<Area3DComponent>() returned nullptr\n", nodeName);
#else
                std::cout << "Area3D.getComponent: Node '" << nodeName << "' found but getComponent<Area3DComponent>() returned nullptr" << std::endl;
#endif
                lua_pushnil(L);
                return 1;
            }
            
            // Create a Lua table representing the Area3D component
            lua_newtable(L); // Stack: -1 = table
            
            // Add properties
            lua_pushstring(L, "nodeName"); // Stack: -2 = table, -1 = "nodeName"
            lua_pushstring(L, nodeName);   // Stack: -3 = table, -2 = "nodeName", -1 = nodeName value
            lua_settable(L, -3); // Pops key and value, sets in table at -3. Stack: -1 = table
            
            // Store nodeName as std::string to ensure it persists
            std::string savedNodeName = std::string(nodeName);
            
            // Add getBodiesInArea function (with nodeName as upvalue)
            lua_pushstring(L, "getBodiesInArea"); // Stack: -2 = table, -1 = "getBodiesInArea"
            lua_pushstring(L, savedNodeName.c_str()); // Stack: -3 = table, -2 = "getBodiesInArea", -1 = upvalue
            lua_pushcclosure(L, [](lua_State* L) -> int {
                            // Get nodeName from upvalue (index 1)
                            const char* nodeName = lua_tostring(L, lua_upvalueindex(1));
                            
                            if (!nodeName) {
                                lua_newtable(L);
                                return 1;
                            }
                            
#ifndef VITA_BUILD
                            try {
#endif
                                auto& engine = GetEngine();
                                auto& sceneManager = engine.getSceneManager();
                                auto activeScene = sceneManager.getCurrentScene();
                                
                                if (activeScene) {
                                    auto node = activeScene->findNode(nodeName);
                                    if (node) {
                                        auto area3DComp = node->getComponent<Area3DComponent>();
                                        if (area3DComp) {
                                            auto bodies = area3DComp->getBodiesInArea();
                                            lua_newtable(L);
                                            for (size_t i = 0; i < bodies.size(); ++i) {
                                                lua_pushinteger(L, i + 1);
                                                lua_pushstring(L, bodies[i].c_str());
                                                lua_settable(L, -3);
                                            }
                                            return 1;
                                        }
                                    }
                                }
#ifndef VITA_BUILD
                            } catch (...) {
                            }
#endif
                            
                            lua_newtable(L);
                            return 1;
                        }, 1); // 1 upvalue (nodeName)
                        // Stack after pushcclosure: -3 = table, -2 = key, -1 = closure
                        lua_settable(L, -3); // Pops key and closure, sets in table. Stack: -1 = table
                        
                        // Add isBodyInArea function (with nodeName as upvalue)
                        lua_pushstring(L, "isBodyInArea"); // Stack: -2 = table, -1 = "isBodyInArea"
                        lua_pushstring(L, savedNodeName.c_str()); // Stack: -3 = table, -2 = "isBodyInArea", -1 = upvalue
                        lua_pushcclosure(L, [](lua_State* L) -> int {
                            // Get nodeName from upvalue (index 1)
                            const char* nodeName = lua_tostring(L, lua_upvalueindex(1));
                            const char* bodyName = luaL_checkstring(L, 1);
                            
                            if (!nodeName || !bodyName) {
                                lua_pushboolean(L, false);
                                return 1;
                            }
                            
#ifndef VITA_BUILD
                            try {
#endif
                                auto& engine = GetEngine();
                                auto& sceneManager = engine.getSceneManager();
                                auto activeScene = sceneManager.getCurrentScene();
                                
                                if (activeScene) {
                                    auto node = activeScene->findNode(nodeName);
                                    if (node) {
                                        auto area3DComp = node->getComponent<Area3DComponent>();
                                        if (area3DComp) {
                                            bool result = area3DComp->isBodyInArea(bodyName);
                                            lua_pushboolean(L, result);
                                            return 1;
                                        }
                                    }
                                }
#ifndef VITA_BUILD
                            } catch (...) {
                            }
#endif
                            
                            lua_pushboolean(L, false);
                            return 1;
                        }, 1); // 1 upvalue (nodeName)
                        // Stack after pushcclosure: -3 = table, -2 = key, -1 = closure
                        lua_settable(L, -3); // Stack: -1 = table
                        
                        // Add getBodyCount function (with nodeName as upvalue)
                        lua_pushstring(L, "getBodyCount"); // Stack: -2 = table, -1 = "getBodyCount"
                        lua_pushstring(L, savedNodeName.c_str()); // Stack: -3 = table, -2 = "getBodyCount", -1 = upvalue
                        lua_pushcclosure(L, [](lua_State* L) -> int {
                            // Get nodeName from upvalue (index 1)
                            const char* nodeName = lua_tostring(L, lua_upvalueindex(1));
                            
                            if (!nodeName) {
                                lua_pushinteger(L, 0);
                                return 1;
                            }
                            
#ifndef VITA_BUILD
                            try {
#endif
                                auto& engine = GetEngine();
                                auto& sceneManager = engine.getSceneManager();
                                auto activeScene = sceneManager.getCurrentScene();
                                
                                if (activeScene) {
                                    auto node = activeScene->findNode(nodeName);
                                    if (node) {
                                        auto area3DComp = node->getComponent<Area3DComponent>();
                                        if (area3DComp) {
                                            size_t count = area3DComp->getBodyCount();
                                            lua_pushinteger(L, count);
                                            return 1;
                                        }
                                    }
                                }
#ifndef VITA_BUILD
                            } catch (...) {
                            }
#endif
                            
                            lua_pushinteger(L, 0);
                            return 1;
                        }, 1); // 1 upvalue (nodeName)
                        // Stack after pushcclosure: -3 = table, -2 = key, -1 = closure
                        lua_settable(L, -3); // Stack: -1 = table
                        
                        // Add getGroup function (with nodeName as upvalue)
                        lua_pushstring(L, "getGroup"); // Stack: -2 = table, -1 = "getGroup"
                        lua_pushstring(L, savedNodeName.c_str()); // Stack: -3 = table, -2 = "getGroup", -1 = upvalue
                        lua_pushcclosure(L, [](lua_State* L) -> int {
                            // Get nodeName from upvalue (index 1)
                            const char* nodeName = lua_tostring(L, lua_upvalueindex(1));
                            
                            if (!nodeName) {
                                lua_pushnil(L);
                                return 1;
                            }
                            
#ifndef VITA_BUILD
                            try {
#endif
                                auto& engine = GetEngine();
                                auto& sceneManager = engine.getSceneManager();
                                auto activeScene = sceneManager.getCurrentScene();
                                
                                if (activeScene) {
                                    auto node = activeScene->findNode(nodeName);
                                    if (node) {
                                        auto area3DComp = node->getComponent<Area3DComponent>();
                                        if (area3DComp && area3DComp->hasGroup()) {
                                            lua_pushstring(L, area3DComp->getGroup().c_str());
                                            return 1;
                                        }
                                    }
                                }
#ifndef VITA_BUILD
                            } catch (...) {
                            }
#endif
                            
                            lua_pushnil(L);
                            return 1;
                        }, 1); // 1 upvalue (nodeName)
                        // Stack after pushcclosure: -3 = table, -2 = key, -1 = closure
                        lua_settable(L, -3); // Stack: -1 = table
                        
                        // Add setMonitorMode function (with nodeName as upvalue)
                        lua_pushstring(L, "setMonitorMode"); // Stack: -2 = table, -1 = "setMonitorMode"
                        lua_pushstring(L, savedNodeName.c_str()); // Stack: -3 = table, -2 = "setMonitorMode", -1 = upvalue
                        lua_pushcclosure(L, [](lua_State* L) -> int {
                            // Get nodeName from upvalue (index 1)
                            const char* nodeName = lua_tostring(L, lua_upvalueindex(1));
                            
                            if (!nodeName) {
                                return 0;
                            }
                            
                            bool enabled = lua_toboolean(L, 1) != 0; // Get boolean argument
                            
#ifndef VITA_BUILD
                            try {
#endif
                                auto& engine = GetEngine();
                                auto& sceneManager = engine.getSceneManager();
                                auto activeScene = sceneManager.getCurrentScene();
                                
                                if (activeScene) {
                                    auto node = activeScene->findNode(nodeName);
                                    if (node) {
                                        auto area3DComp = node->getComponent<Area3DComponent>();
                                        if (area3DComp) {
                                            area3DComp->setMonitorMode(enabled);
                                        }
                                    }
                                }
#ifndef VITA_BUILD
                            } catch (...) {
                            }
#endif
                            
                            return 0; // No return value
                        }, 1); // 1 upvalue (nodeName)
                        // Stack after pushcclosure: -3 = table, -2 = key, -1 = closure
                        lua_settable(L, -3); // Stack: -1 = table
            
            // Verify we're returning a table
            if (!lua_istable(L, -1)) {
#ifdef VITA_BUILD
                printf("ERROR: Not returning a table! Type: %s\n", lua_typename(L, lua_type(L, -1)));
#else
                std::cout << "ERROR: Not returning a table! Type: " << lua_typename(L, lua_type(L, -1)) << std::endl;
#endif
                lua_pop(L, 1); // Remove whatever is on stack
                lua_newtable(L); // Create a new table
            }
            
            return 1; // Return the component table (table is at top of stack)
#ifndef VITA_BUILD
        } catch (...) {
            std::cout << "Area3D.getComponent: Exception caught" << std::endl;
            lua_pushnil(L);
            return 1;
        }
#endif
        
        // Should not reach here - return nil as fallback
        lua_pushnil(L);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Function to get all Area3D components in a group
    lua_pushstring(luaState, "getComponentsInGroup");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* groupName = luaL_checkstring(L, 1);
        
        auto components = Area3DComponent::getComponentsInGroup(groupName);
        lua_newtable(L);
        
        for (size_t i = 0; i < components.size(); ++i) {
            auto comp = components[i];
            if (comp && comp->getOwner()) {
                lua_pushinteger(L, i + 1);
                
                // Create component table
                lua_newtable(L);
                lua_pushstring(L, "nodeName");
                lua_pushstring(L, comp->getOwner()->getName().c_str());
                lua_settable(L, -3);
                
                // Add same functions as getComponent
                // (Simplified - in practice you'd share this code)
                lua_pushstring(L, "getBodyCount");
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* nodeName = nullptr;
                    lua_pushstring(L, "nodeName");
                    lua_gettable(L, -2);
                    if (lua_isstring(L, -1)) {
                        nodeName = lua_tostring(L, -1);
                    }
                    lua_pop(L, 1);
                    
                    if (!nodeName) {
                        lua_pushinteger(L, 0);
                        return 1;
                    }
                    
#ifndef VITA_BUILD
                    try {
#endif
                        auto& engine = GetEngine();
                        auto& sceneManager = engine.getSceneManager();
                        auto activeScene = sceneManager.getCurrentScene();
                        
                        if (activeScene) {
                            auto node = activeScene->findNode(nodeName);
                            if (node) {
                                auto area3DComp = node->getComponent<Area3DComponent>();
                                if (area3DComp) {
                                    lua_pushinteger(L, area3DComp->getBodyCount());
                                    return 1;
                                }
                            }
                        }
#ifndef VITA_BUILD
                    } catch (...) {
                    }
#endif
                    
                    lua_pushinteger(L, 0);
                    return 1;
                });
                lua_settable(L, -3);
                
                lua_settable(L, -3);
            }
        }
        
        return 1; // Return table of components
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "area3D");
    
    // Area3D system bound successfully
}

namespace {
void applyLuaTableToMaterial(GameEngine::Material* mat, lua_State* L, int tableIndex) {
    if (!mat || !lua_istable(L, tableIndex)) return;
    lua_pushnil(L);
    while (lua_next(L, tableIndex) != 0) {
        const char* key = lua_tostring(L, -2);
        if (!key) { lua_pop(L, 1); continue; }
        if (strcmp(key, "blendMode") == 0) {
            const char* mode = lua_tostring(L, -1);
            if (mode) {
                if (strcmp(mode, "Alpha") == 0) mat->setBlendMode(GameEngine::BlendMode::Alpha);
                else if (strcmp(mode, "Additive") == 0) mat->setBlendMode(GameEngine::BlendMode::Additive);
                else mat->setBlendMode(GameEngine::BlendMode::Opaque);
            }
        } else if (strcmp(key, "depthWrite") == 0) {
            mat->setDepthWrite(lua_toboolean(L, -1) != 0);
        } else if (lua_isnumber(L, -1)) {
            mat->setFloat(key, (float)lua_tonumber(L, -1));
        } else if (lua_istable(L, -1)) {
            int len = (int)lua_rawlen(L, -1);
            float v[4] = {0, 0, 0, 0};
            for (int i = 0; i < len && i < 4; i++) {
                lua_rawgeti(L, -1, i + 1);
                v[i] = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
            if (len == 2) mat->setVec2(key, glm::vec2(v[0], v[1]));
            else if (len == 3) mat->setVec3(key, glm::vec3(v[0], v[1], v[2]));
            else if (len >= 4) mat->setVec4(key, glm::vec4(v[0], v[1], v[2], v[3]));
        }
        lua_pop(L, 1);
    }
}
}

void ScriptComponent::bindSceneToLua() {
    if (!luaState) {
        return;
    }
    
    // Create scene table
    lua_newtable(luaState);
    
    // Load scene function - switches to a different scene by name
    lua_pushstring(luaState, "loadScene");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
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
    lua_settable(luaState, -3);
    
    // Load scene from file function - loads a scene from a JSON file
    lua_pushstring(luaState, "loadSceneFromFile");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* sceneName = luaL_checkstring(L, 1);
        const char* filepath = luaL_checkstring(L, 2);
        
        if (!sceneName || !filepath) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        auto& engine = GetEngine();
        auto& sceneManager = engine.getSceneManager();
        
        // Load scene from JSON file
        bool success = sceneManager.loadSceneFromFile(sceneName, filepath);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "setNodeMaterialOverride");
    lua_pushcclosure(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* vertexPath = luaL_optstring(L, 2, "");
        const char* fragmentPath = luaL_optstring(L, 3, "");
        if (!nodeName || !vertexPath || !fragmentPath) { lua_pushboolean(L, false); return 1; }
        auto& engine = GameEngine::GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene) { lua_pushboolean(L, false); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushboolean(L, false); return 1; }
        std::shared_ptr<GameEngine::Material> overrideMat;
        bool hasParams = lua_type(L, 4) == LUA_TTABLE;
        if (hasParams) {
            auto baseMat = engine.getOrCreateMaterialByShaderPaths(vertexPath, fragmentPath);
            if (!baseMat || !baseMat->getShader()) { lua_pushboolean(L, false); return 1; }
            overrideMat = std::make_shared<GameEngine::Material>(baseMat->getShader());
            applyLuaTableToMaterial(overrideMat.get(), L, 4);
        } else {
            overrideMat = engine.getOrCreateMaterialByShaderPaths(vertexPath, fragmentPath);
        }
        if (!overrideMat) { lua_pushboolean(L, false); return 1; }
        // Apply override to this node and its descendants only (not parent + siblings)
        std::function<void(GameEngine::SceneNode*)> visit = [&visit, &overrideMat](GameEngine::SceneNode* n) {
            if (!n) return;
            auto mr = n->getComponent<GameEngine::MeshRenderer>();
            if (mr) mr->setMaterialOverride(overrideMat);
            for (size_t i = 0; i < n->getChildCount(); ++i) {
                auto ch = n->getChild(i);
                if (ch) visit(ch.get());
            }
        };
        visit(node.get());
        lua_pushboolean(L, true);
        return 1;
    }, 0);
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "clearNodeMaterialOverride");
    lua_pushcclosure(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        if (!nodeName) { lua_pushboolean(L, false); return 1; }
        auto activeScene = GameEngine::GetEngine().getSceneManager().getCurrentScene();
        if (!activeScene) { lua_pushboolean(L, false); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushboolean(L, false); return 1; }
        // Clear override on this node and its descendants only (not parent + siblings)
        std::function<void(GameEngine::SceneNode*)> visit = [&visit](GameEngine::SceneNode* n) {
            if (!n) return;
            auto mr = n->getComponent<GameEngine::MeshRenderer>();
            if (mr) mr->setMaterialOverride(nullptr);
            for (size_t i = 0; i < n->getChildCount(); ++i) {
                auto ch = n->getChild(i);
                if (ch) visit(ch.get());
            }
        };
        visit(node.get());
        lua_pushboolean(L, true);
        return 1;
    }, 0);
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "setNodeBlendMode");
    lua_pushcclosure(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* modeStr = luaL_optstring(L, 2, "Opaque");
        if (!nodeName) { lua_pushboolean(L, false); return 1; }
        auto activeScene = GameEngine::GetEngine().getSceneManager().getCurrentScene();
        if (!activeScene) { lua_pushboolean(L, false); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushboolean(L, false); return 1; }
        GameEngine::BlendMode mode = GameEngine::BlendMode::Opaque;
        if (modeStr) {
            if (strcmp(modeStr, "Alpha") == 0) mode = GameEngine::BlendMode::Alpha;
            else if (strcmp(modeStr, "Additive") == 0) mode = GameEngine::BlendMode::Additive;
        }
        bool anySet = false;
        std::function<void(GameEngine::SceneNode*)> visit = [&visit, mode, &anySet](GameEngine::SceneNode* n) {
            if (!n) return;
            auto mr = n->getComponent<GameEngine::MeshRenderer>();
            if (mr) {
                auto mat = mr->getMaterialOverride() ? mr->getMaterialOverride() : mr->getMaterial();
                if (mat) { mat->setBlendMode(mode); anySet = true; }
            }
            for (size_t i = 0; i < n->getChildCount(); ++i) {
                auto ch = n->getChild(i);
                if (ch) visit(ch.get());
            }
        };
        visit(node.get());
        lua_pushboolean(L, anySet);
        return 1;
    }, 0);
    lua_settable(luaState, -3);
    
    // Create a node and attach to parent (empty = root).
    lua_pushstring(luaState, "createNode");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* parentName = luaL_optstring(L, 2, nullptr);
        if (!nodeName) { lua_pushnil(L); return 1; }
        auto& engine = GetEngine();
        auto& sceneManager = engine.getSceneManager();
        auto activeScene = sceneManager.getCurrentScene();
        if (!activeScene) { lua_pushnil(L); return 1; }
        std::shared_ptr<SceneNode> node = activeScene->createNode(nodeName);
        if (!node) { lua_pushnil(L); return 1; }
        if (parentName && parentName[0] != '\0') {
            std::shared_ptr<SceneNode> parent = activeScene->findNode(parentName);
            if (parent) {
                parent->addChild(node);
            } else {
                activeScene->addNode(node);
            }
        } else {
            activeScene->addNode(node);
        }
        lua_pushstring(L, node->getName().c_str());
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Add a mesh to an existing node
    lua_pushstring(luaState, "addMeshToNode");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* meshShape = luaL_checkstring(L, 2);
        if (!nodeName || !meshShape) { lua_pushboolean(L, false); return 1; }
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene) { lua_pushboolean(L, false); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushboolean(L, false); return 1; }
        if (node->getComponent<MeshRenderer>()) { lua_pushboolean(L, false); return 1; }
        std::shared_ptr<Mesh> mesh;
        std::string shape(meshShape);
        if (shape == "sphere") {
            float radius = luaL_optnumber(L, 3, 0.5f);
            mesh = Mesh::createSphere(16, 16, radius);
        } else if (shape == "cube") {
            mesh = Mesh::createCube();
        } else if (shape == "capsule") {
            float radius = luaL_optnumber(L, 3, 0.5f);
            float halfHeight = luaL_optnumber(L, 4, 0.5f);
            mesh = Mesh::createCapsule(radius, halfHeight, 16, 8);
        } else if (shape == "cylinder") {
            float radius = luaL_optnumber(L, 3, 0.5f);
            float halfHeight = luaL_optnumber(L, 4, 0.5f);
            mesh = Mesh::createCylinder(radius, halfHeight, 16);
        } else if (shape == "plane") {
            float w = luaL_optnumber(L, 3, 1.0f);
            float h = luaL_optnumber(L, 4, 1.0f);
            mesh = Mesh::createPlane(w, h, 1);
        } else if (shape == "quad") {
            mesh = Mesh::createQuad();
        } else {
            lua_pushboolean(L, false);
            return 1;
        }
        if (!mesh) { lua_pushboolean(L, false); return 1; }
        auto lightingShader = Shader::getLightingShader();
        if (!lightingShader || !lightingShader->isValid()) { lua_pushboolean(L, false); return 1; }
        auto mat = std::make_shared<Material>();
        mat->setShader(lightingShader);
        mat->setColor(glm::vec3(0.85f, 0.2f, 0.2f)); // reddish so the ball is visible
        auto* mr = node->addComponent<MeshRenderer>();
        if (!mr) { lua_pushboolean(L, false); return 1; }
        mr->setMesh(mesh);
        mr->setMaterial(mat);
        lua_pushboolean(L, true);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Add physics to an existing node
    lua_pushstring(luaState, "addPhysicsToNode");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* bodyTypeStr = luaL_checkstring(L, 2);
        const char* shapeTypeStr = luaL_checkstring(L, 3);
        if (!nodeName || !bodyTypeStr || !shapeTypeStr) { lua_pushboolean(L, false); return 1; }
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (!activeScene) { lua_pushboolean(L, false); return 1; }
        auto node = activeScene->findNode(nodeName);
        if (!node) { lua_pushboolean(L, false); return 1; }
        if (node->getComponent<PhysicsComponent>()) { lua_pushboolean(L, false); return 1; }
        PhysicsBodyType bodyType = PhysicsBodyType::STATIC;
        std::string bt(bodyTypeStr);
        if (bt == "dynamic") bodyType = PhysicsBodyType::DYNAMIC;
        else if (bt == "kinematic") bodyType = PhysicsBodyType::KINEMATIC;
        CollisionShapeType shapeType = CollisionShapeType::BOX;
        glm::vec3 dims(1.0f);
        int massArg = 7;
        std::string st(shapeTypeStr);
        if (st == "box") {
            shapeType = CollisionShapeType::BOX;
            dims.x = luaL_optnumber(L, 4, 1.0f);
            dims.y = luaL_optnumber(L, 5, 1.0f);
            dims.z = luaL_optnumber(L, 6, 1.0f);
            massArg = 7;
        } else if (st == "sphere") {
            shapeType = CollisionShapeType::SPHERE;
            float r = luaL_optnumber(L, 4, 0.5f);
            dims = glm::vec3(r * 2.0f, r * 2.0f, r * 2.0f);
            massArg = 5;
        } else if (st == "capsule") {
            shapeType = CollisionShapeType::CAPSULE;
            float r = luaL_optnumber(L, 4, 0.5f);
            float h = luaL_optnumber(L, 5, 1.0f);
            dims = glm::vec3(r * 2.0f, h, r * 2.0f);
            massArg = 6;
        } else if (st == "cylinder") {
            shapeType = CollisionShapeType::CYLINDER;
            float r = luaL_optnumber(L, 4, 0.5f);
            float halfH = luaL_optnumber(L, 5, 0.5f);
            dims = glm::vec3(r * 2.0f, halfH * 2.0f, r * 2.0f);
            massArg = 6;
        } else if (st == "plane") {
            shapeType = CollisionShapeType::PLANE;
            dims.x = luaL_optnumber(L, 4, 10.0f);
            dims.z = luaL_optnumber(L, 5, 10.0f);
            massArg = 6;
        }
        auto* phys = node->addComponent<PhysicsComponent>();
        if (!phys) { lua_pushboolean(L, false); return 1; }
        phys->setCollisionShape(shapeType, dims);
        phys->setBodyType(bodyType);
        if (bodyType == PhysicsBodyType::DYNAMIC) {
            phys->setMass(luaL_optnumber(L, massArg, 1.0f));
        }
        phys->start();
        lua_pushboolean(L, true);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Queue node for removal at start of next update
    lua_pushstring(luaState, "destroyNode");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        if (!nodeName) return 0;
        auto& engine = GetEngine();
        auto activeScene = engine.getSceneManager().getCurrentScene();
        if (activeScene) {
            activeScene->queueRemoveNode(nodeName);
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "scene");
    
    // Global destroyNode for convenience
    lua_getglobal(luaState, "scene");
    lua_getfield(luaState, -1, "destroyNode");
    lua_setglobal(luaState, "destroyNode");
    lua_pop(luaState, 1);
}

void ScriptComponent::bindAnimationToLua() {
    if (!luaState) {
        return;
    }
    
    // Create animation table
    lua_newtable(luaState);
    
    // Add AnimationComponent to a node
    lua_pushstring(luaState, "addComponent");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->addComponent<AnimationComponent>();
                    if (animComp) {
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Set skeleton for animation component
    lua_pushstring(luaState, "setSkeleton");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* skeletonName = luaL_checkstring(L, 2);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName && skeletonName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        animComp->setSkeleton(skeletonName);
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Set animation clip
    lua_pushstring(luaState, "setAnimationClip");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        const char* clipName = luaL_checkstring(L, 2);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName && clipName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        animComp->setAnimationClip(clipName);
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Play animation
    lua_pushstring(luaState, "play");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        animComp->play();
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Pause animation
    lua_pushstring(luaState, "pause");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        animComp->pause();
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "isPlaying");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        lua_pushboolean(L, animComp->isPlaying());
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Stop animation
    lua_pushstring(luaState, "stop");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        animComp->stop();
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Set animation speed
    lua_pushstring(luaState, "setSpeed");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        float speed = luaL_checknumber(L, 2);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        animComp->setSpeed(speed);
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Set animation loop
    lua_pushstring(luaState, "setLoop");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        bool loop = lua_toboolean(L, 2) != 0;
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        animComp->setLoop(loop);
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Get available skeletons
    lua_pushstring(luaState, "getAvailableSkeletons");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
#ifndef VITA_BUILD
        try {
#endif
            auto& animManager = AnimationManager::getInstance();
            auto skeletons = animManager.getAvailableSkeletons();
            
            lua_newtable(L);
            for (size_t i = 0; i < skeletons.size(); ++i) {
                lua_pushinteger(L, i + 1);
                lua_pushstring(L, skeletons[i].c_str());
                lua_settable(L, -3);
            }
            return 1;
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_newtable(L);
        return 1;
    });
    lua_settable(luaState, -3);
    
    // Get available animation clips for a skeleton
    lua_pushstring(luaState, "getAnimationClips");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* skeletonName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            if (skeletonName) {
                auto& animManager = AnimationManager::getInstance();
                auto clips = animManager.getAnimationClipsForSkeleton(skeletonName);
                
                lua_newtable(L);
                for (size_t i = 0; i < clips.size(); ++i) {
                    lua_pushinteger(L, i + 1);
                    lua_pushstring(L, clips[i].c_str());
                    lua_settable(L, -3);
                }
                return 1;
            }
#ifndef VITA_BUILD
        } catch (...) {
        }
#endif
        
        lua_newtable(L);
        return 1;
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "animation");
}

void ScriptComponent::bindSoundToLua() {
    if (!luaState) {
        return;
    }
    
    // Create sound table
    lua_newtable(luaState);
    
    // Function to get SoundComponent from a node by name
    lua_pushstring(luaState, "getComponent");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (!activeScene || !nodeName) {
                lua_pushnil(L);
                return 1;
            }
            
            auto node = activeScene->findNode(nodeName);
            if (!node) {
                lua_pushnil(L);
                return 1;
            }
            
            auto soundComponent = node->getComponent<SoundComponent>();
            if (!soundComponent) {
                std::cout << "SoundComponent::getComponent() - No SoundComponent found on node: " << nodeName << std::endl;
                lua_pushnil(L);
                return 1;
            }
            
            std::cout << "SoundComponent::getComponent() - Found SoundComponent on node: " << nodeName << std::endl;
            
            // Create a table with sound component methods
            lua_newtable(L);
            
            // Store the component pointer FIRST (before adding methods)
            lua_pushstring(L, "_soundComponent");
            lua_pushlightuserdata(L, soundComponent);
            lua_settable(L, -3);
            
            // play()
            lua_pushstring(L, "play");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                // Check if first argument is a table
                if (!lua_istable(L, 1)) {
                    std::cerr << "SoundComponent::play() - First argument is not a table!" << std::endl;
                    return 0;
                }
                
                lua_getfield(L, 1, "_soundComponent");
                if (lua_isnil(L, -1)) {
                    std::cerr << "SoundComponent::play() - _soundComponent field is nil!" << std::endl;
                    lua_pop(L, 1);
                    return 0;
                }
                
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    comp->play();
                } else {
                    std::cerr << "SoundComponent::play() - Component pointer is null!" << std::endl;
                }
                return 0;
            });
            lua_settable(L, -3);
            
            // pause()
            lua_pushstring(L, "pause");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    comp->pause();
                }
                return 0;
            });
            lua_settable(L, -3);
            
            // resume()
            lua_pushstring(L, "resume");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    comp->resume();
                }
                return 0;
            });
            lua_settable(L, -3);
            
            // stop()
            lua_pushstring(L, "stop");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    comp->stop();
                }
                return 0;
            });
            lua_settable(L, -3);
            
            // setVolume(volume)
            lua_pushstring(L, "setVolume");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp && lua_isnumber(L, 2)) {
                    float volume = lua_tonumber(L, 2);
                    comp->setVolume(volume);
                }
                return 0;
            });
            lua_settable(L, -3);
            
            // getVolume()
            lua_pushstring(L, "getVolume");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    lua_pushnumber(L, comp->getVolume());
                    return 1;
                }
                lua_pushnumber(L, 0.0);
                return 1;
            });
            lua_settable(L, -3);
            
            // setLoop(loop)
            lua_pushstring(L, "setLoop");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp && lua_isboolean(L, 2)) {
                    bool loop = lua_toboolean(L, 2) != 0;
                    comp->setLoop(loop);
                }
                return 0;
            });
            lua_settable(L, -3);
            
            // isLooping()
            lua_pushstring(L, "isLooping");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    lua_pushboolean(L, comp->isLooping());
                    return 1;
                }
                lua_pushboolean(L, false);
                return 1;
            });
            lua_settable(L, -3);
            
            // isPlaying()
            lua_pushstring(L, "isPlaying");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    lua_pushboolean(L, comp->isPlaying());
                    return 1;
                }
                lua_pushboolean(L, false);
                return 1;
            });
            lua_settable(L, -3);
            
            // setPitch(pitch)
            lua_pushstring(L, "setPitch");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp && lua_isnumber(L, 2)) {
                    float pitch = lua_tonumber(L, 2);
                    comp->setPitch(pitch);
                }
                return 0;
            });
            lua_settable(L, -3);
            
            // getPitch()
            lua_pushstring(L, "getPitch");
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_getfield(L, 1, "_soundComponent");
                SoundComponent* comp = static_cast<SoundComponent*>(lua_touserdata(L, -1));
                lua_pop(L, 1);
                
                if (comp) {
                    lua_pushnumber(L, comp->getPitch());
                    return 1;
                }
                lua_pushnumber(L, 1.0);
                return 1;
            });
            lua_settable(L, -3);
            
            return 1;
#ifndef VITA_BUILD
        } catch (...) {
            lua_pushnil(L);
            return 1;
        }
#endif
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "play");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        
        if (self && self->owner) {
            auto soundComponent = self->owner->getComponent<SoundComponent>();
            if (soundComponent) {
                soundComponent->play();
            }
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "pause");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        
        if (self && self->owner) {
            auto soundComponent = self->owner->getComponent<SoundComponent>();
            if (soundComponent) {
                soundComponent->pause();
            }
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_pushstring(luaState, "stop");
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        lua_getglobal(L, "_currentScriptComponent");
        ScriptComponent* self = static_cast<ScriptComponent*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        
        if (self && self->owner) {
            auto soundComponent = self->owner->getComponent<SoundComponent>();
            if (soundComponent) {
                soundComponent->stop();
            }
        }
        return 0;
    });
    lua_settable(luaState, -3);
    
    lua_setglobal(luaState, "sound");
}

void ScriptComponent::bindSkyboxToLua() {
    if (!luaState) {
        return;
    }
    
    // Set active skybox by node name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto skyboxComp = node->getComponent<SkyboxComponent>();
                    if (skyboxComp) {
                        activeScene->setActiveSkybox(node);
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setActiveSkybox");
    
    // Get active skybox node name
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene) {
                auto skyboxNode = activeScene->getActiveSkybox();
                if (skyboxNode) {
                    lua_pushstring(L, skyboxNode->getName().c_str());
                    return 1;
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        lua_pushnil(L);
        return 1;
    });
    lua_setglobal(luaState, "getActiveSkybox");
    
    // Set skybox textures
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        
        if (!lua_istable(L, 2)) {
            lua_pushboolean(L, false);
            return 1;
        }
        
        std::vector<std::string> paths(6);
        bool allSet = true;
        
        // Get 6 texture paths
        for (int i = 0; i < 6; i++) {
            lua_pushinteger(L, i + 1);
            lua_gettable(L, 2);
            
            if (lua_isstring(L, -1)) {
                paths[i] = lua_tostring(L, -1);
            } else {
                allSet = false;
            }
            
            lua_pop(L, 1);
        }
        
        if (!allSet) {
            lua_pushboolean(L, false);
            return 1;
        }
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto skyboxComp = node->getComponent<SkyboxComponent>();
                    if (skyboxComp) {
                        bool result = skyboxComp->setTextures(paths);
                        lua_pushboolean(L, result);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setSkyboxTextures");
    
    // Enable/disable skybox
    lua_pushcfunction(luaState, [](lua_State* L) -> int {
        const char* nodeName = luaL_checkstring(L, 1);
        bool enabled = lua_toboolean(L, 2) != 0;
        
#ifndef VITA_BUILD
        try {
#endif
            auto& engine = GetEngine();
            auto& sceneManager = engine.getSceneManager();
            auto activeScene = sceneManager.getCurrentScene();
            
            if (activeScene && nodeName) {
                auto node = activeScene->findNode(nodeName);
                if (node) {
                    auto skyboxComp = node->getComponent<SkyboxComponent>();
                    if (skyboxComp) {
                        if (enabled) {
                            activeScene->setActiveSkybox(node);
                        } else {
                            skyboxComp->setActive(false);
                        }
                        lua_pushboolean(L, true);
                        return 1;
                    }
                }
            }
#ifndef VITA_BUILD
        } catch (...) {
            // Handle exceptions gracefully
        }
#endif
        
        lua_pushboolean(L, false);
        return 1;
    });
    lua_setglobal(luaState, "setSkyboxEnabled");
}

} // namespace GameEngine
