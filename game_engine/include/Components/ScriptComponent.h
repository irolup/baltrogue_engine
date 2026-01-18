#ifndef SCRIPT_COMPONENT_H
#define SCRIPT_COMPONENT_H

#include "Components/Component.h"
#include <string>
#include <memory>

struct lua_State;

namespace GameEngine {

class ScriptComponent : public Component {
public:
    ScriptComponent();
    virtual ~ScriptComponent();
    
    COMPONENT_TYPE(ScriptComponent)
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void render(Renderer& renderer) override;
    virtual void destroy() override;
    
    bool loadScript(const std::string& scriptPath);
    void reloadScript();
    bool isScriptLoaded() const { return scriptLoaded; }
    
    const std::string& getScriptPath() const { return scriptPath; }
    void setScriptPath(const std::string& path);
    bool isPauseExempt() const { return pauseExempt; }
    void setPauseExempt(bool exempt) { pauseExempt = exempt; }
    
    lua_State* getLuaState() { return luaState; }
    
    virtual void drawInspector() override;
    
    void callScriptFunction(const std::string& functionName);
    void callScriptFunction(const std::string& functionName, float param);
    
    void setScriptProperty(const std::string& name, const std::string& value);
    std::string getScriptProperty(const std::string& name) const;
    
private:
    lua_State* luaState;
    std::string scriptPath;
    bool scriptLoaded;
    bool scriptStarted;
    bool pauseExempt = false;
    
    bool hasScriptFunction(const std::string& functionName);
    
    void handleLuaError(const std::string& operation);
    
    bool initializeLuaState();
    void cleanupLuaState();
    
    void bindEngineToLua();
    void bindCommonFunctions();
    void bindTransformToLua();
    void bindPickupZoneToLua();
    void bindArea3DToLua();
    void bindInputToLua();
    void bindCameraToLua();
    void bindPhysicsToLua();
    void bindRendererToLua();
    void bindSceneToLua();
    void bindAnimationToLua();
};

} // namespace GameEngine

#endif // SCRIPT_COMPONENT_H
