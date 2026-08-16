#ifndef SCRIPT_COMPONENT_H
#define SCRIPT_COMPONENT_H

#include "Components/Component.h"
#include "Core/Ray.h"
#include <string>
#include <memory>

struct lua_State;

namespace GameEngine {

class Scene;

class ScriptComponent : public Component {
public:
    ScriptComponent();
    virtual ~ScriptComponent();
    
    COMPONENT_TYPE(ScriptComponent)

    static void installEngineBindings(lua_State* luaState);
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void fixedUpdate(float deltaTime) override;
    virtual void lateUpdate(float deltaTime) override;
    virtual void render(IRenderer& renderer) override;
    virtual void destroy() override;
    virtual void suspend() override;
    virtual void resume() override;
    
    bool loadScript(const std::string& scriptPath);
    void reloadScript();
    bool isScriptLoaded() const { return scriptLoaded; }

    const std::string& getScriptPath() const { return scriptPath; }
    void setScriptPath(const std::string& path);
    void assignScriptPath(const std::string& path);
    void setOwningScene(Scene* scene);
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
    Scene* owningScene;
    std::string scriptPath;
    bool scriptLoaded;
    bool scriptStarted;
    bool pauseExempt = false;
    
    bool hasScriptFunction(const std::string& functionName);
    bool ensureScriptLoaded();
    
    void handleLuaError(const std::string& operation);
    
    bool initializeLuaState();
    void cleanupLuaState();
    
    void bindEngineToLua();
    void bindCommonFunctions();
    void bindTransformToLua();
    void bindArea3DToLua();

    // Screen point -> world ray through the active camera, sized to the window.
    static bool screenPointRay(const glm::vec2& screenPoint, Ray& outRay);
    static bool pointerRay(Ray& outRay);

    void bindInputToLua();
    void bindCameraToLua();
    void bindPhysicsToLua();
    void bindUiToLua();
    void bindNavToLua();
    void bindRendererToLua();
    void bindSceneToLua();
    void bindAnimationToLua();
    void bindSoundToLua();
    void bindSkyboxToLua();
    void bindSaveFileToLua();
};

} // namespace GameEngine

#endif // SCRIPT_COMPONENT_H
