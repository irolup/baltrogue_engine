#ifndef SCRIPT_MANAGER_H
#define SCRIPT_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

extern "C" {
    #include <lua.h>
}

namespace GameEngine {

class Engine;
class SceneNode;
class Component;
class InputManager;
class PhysicsManager;
class Renderer;

class ScriptManager {
public:
    static ScriptManager& getInstance();
    
    bool initialize();
    void shutdown();
    ~ScriptManager();
    
    bool executeScript(const std::string& scriptPath);
    bool executeScriptString(const std::string& scriptCode);
    
    void enableHotReload(bool enable) { hotReloadEnabled = enable; }
    void hotReloadScript(const std::string& scriptPath);
    void hotReloadAllScripts();
    
    lua_State* getGlobalLuaState() { return globalLuaState; }
    
    void watchScriptFile(const std::string& scriptPath);
    void unwatchScriptFile(const std::string& scriptPath);
    
    void bindEngineSystems();
    void bindInputSystem();
    void bindPhysicsSystem();
    void bindRendererSystem();
    void bindSceneSystem();
    void bindPickupZoneSystem();
    void bindMenuSystem();
    
    std::string getScriptDirectory() const { return scriptDirectory; }
    void setScriptDirectory(const std::string& dir) { scriptDirectory = dir; }
    
    void setErrorCallback(std::function<void(const std::string&)> callback);
    
private:
    ScriptManager();
    
    static std::unique_ptr<ScriptManager> instance;
    
    lua_State* globalLuaState;
    
    std::string scriptDirectory;
    bool hotReloadEnabled;
    bool initialized;
    
    std::unordered_map<std::string, time_t> scriptFileTimes;
    
    std::function<void(const std::string&)> errorCallback;
    
    bool initializeLua();
    void cleanupLua();
    
    void bindCommonFunctions();
    void bindMathFunctions();
    void bindUtilityFunctions();
    
    time_t getFileModificationTime(const std::string& filePath);
    bool hasFileChanged(const std::string& filePath);
    
    void handleLuaError(const std::string& operation);
    static int luaErrorHandler(lua_State* L);
};

} // namespace GameEngine

#endif // SCRIPT_MANAGER_H
