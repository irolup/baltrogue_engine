#ifndef EDITOR_SYSTEM_H
#define EDITOR_SYSTEM_H

#ifdef LINUX_BUILD

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "Rendering/Framebuffer.h"
#include "Components/CameraComponent.h"
#include "Editor/SceneSerializer.h"
#include "Editor/BuildSystem.h"

namespace GameEngine {

class Scene;
class SceneNode;
class Renderer;
class EditorUI;

class EditorSystem {
    friend class EditorUI;  // Allow EditorUI to access private members
public:
    EditorSystem();
    ~EditorSystem();
    
    bool initialize();
    void shutdown();
    
    void update(float deltaTime);
    void render();
    
    std::shared_ptr<Scene> getActiveScene() const { return activeScene; }
    void setActiveScene(std::shared_ptr<Scene> scene);
    
    enum class CameraMode {
        EDITOR_CAMERA,
        GAME_CAMERA
    };
    
    CameraMode getCameraMode() const { return cameraMode; }
    void setCameraMode(CameraMode mode);
    std::shared_ptr<SceneNode> getEditorCamera() const { return editorCamera; }
    std::shared_ptr<SceneNode> getActiveCamera() const;
    
    void selectNode(std::shared_ptr<SceneNode> node);
    std::shared_ptr<SceneNode> getSelectedNode() const { return selectedNode.lock(); }
    void clearSelection();
    
    bool isViewportFocused() const { return viewportFocused; }
    void setViewportFocused(bool focused) { viewportFocused = focused; }
    bool isAnyWindowHovered() const;
    
    std::string generateUniqueNodeName(const std::string& baseName);
    void deleteNode(std::shared_ptr<SceneNode> node);
    int getNodeDepth(std::shared_ptr<SceneNode> node);
    void moveNodeUp(std::shared_ptr<SceneNode> node);
    void moveNodeDown(std::shared_ptr<SceneNode> node);
    void selectAllChildren(std::shared_ptr<SceneNode> node);
    
    bool saveSceneToFile(const std::string& filepath);
    bool loadSceneFromFile(const std::string& filepath);
    void createNewScene();
    
    glm::vec2 getViewportSize() const { return viewportSize; }
    void setViewportSize(const glm::vec2& size);
    std::unique_ptr<Framebuffer>& getViewportFramebuffer() { return viewportFramebuffer; }

private:
    std::shared_ptr<Scene> activeScene;
    std::weak_ptr<SceneNode> selectedNode;
    
    CameraMode cameraMode;
    std::shared_ptr<SceneNode> editorCamera;
    
    bool viewportFocused;
    
    std::unique_ptr<Framebuffer> viewportFramebuffer;
    glm::vec2 viewportSize;
    
    std::unique_ptr<EditorUI> ui;
    
    void createDefaultScene();
    void handleViewportInput();
    void handleGameCameraInput(float deltaTime);
    void renderSceneToViewport();
    
    void renderSceneDirectly(Scene& scene, CameraComponent* camera);
    void renderNodeDirectly(std::shared_ptr<SceneNode> node, const glm::mat4& parentTransform,
                          const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, bool isEditorCamera = false);
    void renderSkyboxDirectly(Scene& scene, CameraComponent* camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
    void renderPhysicsDebugShapes(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
};

} // namespace GameEngine

#endif // LINUX_BUILD
#endif // EDITOR_SYSTEM_H
