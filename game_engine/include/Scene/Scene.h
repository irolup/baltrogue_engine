#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Scene/SceneNode.h"
#include "Rendering/IRenderer.h"

struct lua_State;

namespace GameEngine {

class SceneScriptRuntime;

class Renderer;
class Camera;

class Scene {
public:
    Scene(const std::string& name = "Scene");
    ~Scene();
    
    std::shared_ptr<SceneNode> getRootNode() const { return rootNode; }
    std::shared_ptr<SceneNode> createNode(const std::string& name = "Node");
    
    void addNode(std::shared_ptr<SceneNode> node);
    void removeNode(std::shared_ptr<SceneNode> node);
    void removeNode(const std::string& name);
    void queueRemoveNode(const std::string& name);
    
    std::shared_ptr<SceneNode> findNode(const std::string& name);
    std::vector<std::shared_ptr<SceneNode>> findNodesByTag(const std::string& tag);

    void registerNodeTree(const std::shared_ptr<SceneNode>& node);
    void unregisterNodeTree(const std::shared_ptr<SceneNode>& node);
    void onNodeRenamed(SceneNode* node, const std::string& oldName, const std::string& newName);
    void clearNodeNameIndex();
    
    void start();
    void update(float deltaTime);
    void fixedUpdate(float deltaTime);
    void lateUpdate(float deltaTime);
    void render(IRenderer& renderer);
    void destroy();
    void restart();
    void suspend();
    void resume();

    const std::string& getName() const { return name; }
    void setName(const std::string& newName) { name = newName; }

    const std::string& getSourceFilepath() const { return sourceFilepath; }
    void setSourceFilepath(const std::string& filepath) { sourceFilepath = filepath; }

    SceneScriptRuntime& getScriptRuntime();

    SceneScriptRuntime* getScriptRuntimeIfExists() const { return scriptRuntime_.get(); }
    void releaseScriptRuntime();

    bool hasEverStarted() const { return hasEverStarted_; }
    void markEverStarted() { hasEverStarted_ = true; }

    bool isSuspended() const { return suspended_; }
    
    std::shared_ptr<SceneNode> getActiveCamera() const { return activeCamera.lock(); }
    void setActiveCamera(std::shared_ptr<SceneNode> cameraNode);
    std::shared_ptr<SceneNode> getActiveGameCamera() const;
    std::vector<std::shared_ptr<SceneNode>> getActiveGameCameras() const;
    void setCameraActive(std::shared_ptr<SceneNode> cameraNode, bool active);
    std::shared_ptr<SceneNode> getActiveSkybox() const { return activeSkybox.lock(); }
    void setActiveSkybox(std::shared_ptr<SceneNode> skyboxNode);
    
    void setSelectedNode(std::shared_ptr<SceneNode> node);
    std::shared_ptr<SceneNode> getSelectedNode() const { return selectedNode.lock(); }
    void clearSelection();
    
    size_t getNodeCount() const;
    
    bool saveToFile(const std::string& filepath) const;
    bool loadFromFile(const std::string& filepath);
    
private:
    std::string name;
    std::string sourceFilepath;
    // Declared before rootNode so it is destroyed after the node tree
    // ScriptComponent destructors unload their scripts through this runtime.
    std::unique_ptr<SceneScriptRuntime> scriptRuntime_;
    std::shared_ptr<SceneNode> rootNode;
    bool hasEverStarted_ = false;
    bool suspended_ = false;
    std::weak_ptr<SceneNode> activeCamera;
    std::weak_ptr<SceneNode> activeSkybox;
    std::weak_ptr<SceneNode> selectedNode;
    std::unordered_map<std::string, std::weak_ptr<SceneNode>> nodeByName_;
    
    size_t nodeCounter;
    std::vector<std::string> pendingNodeRemovals;
    
    std::string generateUniqueName(const std::string& baseName);
    size_t countNodes(const std::shared_ptr<SceneNode>& node) const;
    void processPendingRemovals();
    void prepareComponentsForRestart(const std::shared_ptr<SceneNode>& node);
    void assignScriptComponentsToScene(const std::shared_ptr<SceneNode>& node);
    void registerNode(const std::shared_ptr<SceneNode>& node);
    void unregisterNode(const std::shared_ptr<SceneNode>& node);
};

} // namespace GameEngine

#endif // SCENE_H
