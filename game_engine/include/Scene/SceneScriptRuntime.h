#ifndef SCENE_SCRIPT_RUNTIME_H
#define SCENE_SCRIPT_RUNTIME_H

#include <string>

struct lua_State;

namespace GameEngine {

class ScriptComponent;

class SceneScriptRuntime {
public:
    SceneScriptRuntime();
    ~SceneScriptRuntime();

    SceneScriptRuntime(const SceneScriptRuntime&) = delete;
    SceneScriptRuntime& operator=(const SceneScriptRuntime&) = delete;

    bool isInitialized() const;
    lua_State* getLuaState();

    bool ensureInitialized();
    void shutdown();

    bool loadScript(ScriptComponent* component, const std::string& path);
    void unloadScript(ScriptComponent* component);

    bool hasFunction(ScriptComponent* component, const std::string& name) const;
    bool callFunction(ScriptComponent* component, const std::string& name);
    bool callFunction(ScriptComponent* component, const std::string& name, float param);

private:
    lua_State* luaState;
    bool engineBindingsInstalled;

    void setActiveComponent(ScriptComponent* component);
    bool pushScriptEnvironment(ScriptComponent* component) const;
    bool pushScriptFunction(ScriptComponent* component, const std::string& name) const;
};

}

#endif
