#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Scene/SceneNode.h"

namespace GameEngine {

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
    
    std::shared_ptr<SceneNode> findNode(const std::string& name);
    std::vector<std::shared_ptr<SceneNode>> findNodesByTag(const std::string& tag);
    
    void start();
    void update(float deltaTime);
    void render(Renderer& renderer);
    void destroy();
    
    const std::string& getName() const { return name; }
    void setName(const std::string& newName) { name = newName; }
    
    std::shared_ptr<SceneNode> getActiveCamera() const { return activeCamera.lock(); }
    void setActiveCamera(std::shared_ptr<SceneNode> cameraNode);
    std::shared_ptr<SceneNode> getActiveGameCamera() const;
    
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
    std::shared_ptr<SceneNode> rootNode;
    std::weak_ptr<SceneNode> activeCamera;
    std::weak_ptr<SceneNode> activeSkybox;
    std::weak_ptr<SceneNode> selectedNode;
    
    size_t nodeCounter;
    
    std::string generateUniqueName(const std::string& baseName);
    size_t countNodes(const std::shared_ptr<SceneNode>& node) const;
};

} // namespace GameEngine

#endif // SCENE_H
