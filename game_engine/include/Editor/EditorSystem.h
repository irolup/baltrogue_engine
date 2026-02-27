#ifndef EDITOR_SYSTEM_H
#define EDITOR_SYSTEM_H

#ifdef LINUX_BUILD

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "Rendering/Framebuffer.h"
#include "Rendering/Shader.h"
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

    void setGridOrigin(const glm::vec3& origin) { gridOrigin = origin; gridMeshDirty = true; }
    glm::vec3 getGridOrigin() const { return gridOrigin; }
    void setGridCellSize(float size) { gridCellSize = size > 0.0f ? size : 1.0f; gridMeshDirty = true; }
    float getGridCellSize() const { return gridCellSize; }
    void setGridSize(int sizeX, int sizeZ);
    int getGridSizeX() const { return gridSizeX; }
    int getGridSizeZ() const { return gridSizeZ; }
    bool isGridLockEnabled() const { return gridLockEnabled; }
    void setGridLockEnabled(bool enabled) { gridLockEnabled = enabled; }
    bool isShowGridEnabled() const { return showGrid; }
    void setShowGridEnabled(bool enabled) { showGrid = enabled; }
    glm::vec3 snapWorldToGrid(const glm::vec3& worldPos, bool snapY = false) const;
    void worldToGridCell(const glm::vec3& worldPos, int& outGx, int& outGz) const;
    glm::vec3 gridCellToWorld(int gx, int gz, float y = 0.0f) const;
    bool isGridCellInBounds(int gx, int gz) const;

private:
    void buildGridMesh();
    void renderGridInViewport(CameraComponent* camera);
    std::shared_ptr<Scene> activeScene;
    std::weak_ptr<SceneNode> selectedNode;
    
    CameraMode cameraMode;
    std::shared_ptr<SceneNode> editorCamera;
    
    bool viewportFocused;
    
    std::unique_ptr<Framebuffer> viewportFramebuffer;
    glm::vec2 viewportSize;
    
    std::unique_ptr<EditorUI> ui;

    glm::vec3 gridOrigin;
    float gridCellSize;
    int gridSizeX;
    int gridSizeZ;
    bool gridLockEnabled;
    bool showGrid;
    bool gridMeshDirty;

    GLuint gridVao;
    GLuint gridVbo;
    GLuint gridIbo;
    GLsizei gridLineCount;
    std::shared_ptr<Shader> gridShader;

    void createDefaultScene();
    void handleViewportInput();
    void handleGameCameraInput(float deltaTime);
    void renderSceneToViewport();
    
    void renderSceneDirectly(Scene& scene, CameraComponent* camera);
    void renderNodeDirectly(std::shared_ptr<SceneNode> node, const glm::mat4& parentTransform, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, bool isEditorCamera, int renderPass);
    void renderSkyboxDirectly(Scene& scene, CameraComponent* camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
    void renderPhysicsDebugShapes(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
};

} // namespace GameEngine

#endif // LINUX_BUILD
#endif // EDITOR_SYSTEM_H
