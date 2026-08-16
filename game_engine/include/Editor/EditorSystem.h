#ifndef EDITOR_SYSTEM_H
#define EDITOR_SYSTEM_H

#ifdef LINUX_BUILD

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Rendering/Framebuffer.h"
#include "Rendering/Shader.h"
#include "Rendering/ShadowAtlasGL.h"
#include "Components/CameraComponent.h"
#include "Editor/SceneSerializer.h"
#include "Editor/BuildSystem.h"
#include "Editor/NodeTemplateSerializer.h"

namespace GameEngine {

class Scene;
class SceneNode;
class Renderer;
class EditorUI;
class Mesh;

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
    bool isViewportHovered() const { return viewportHovered; }
    void setViewportHovered(bool hovered) { viewportHovered = hovered; }
    bool isCameraFlyActive() const { return cameraFlyActive; }
    bool isAnyWindowHovered() const;
    
    std::string generateUniqueNodeName(const std::string& baseName);
    void deleteNode(std::shared_ptr<SceneNode> node);
    int getNodeDepth(std::shared_ptr<SceneNode> node);
    int getSiblingIndex(std::shared_ptr<SceneNode> node) const;
    bool canMoveNodeUp(std::shared_ptr<SceneNode> node) const;
    bool canMoveNodeDown(std::shared_ptr<SceneNode> node) const;
    void moveNodeUp(std::shared_ptr<SceneNode> node);
    void moveNodeDown(std::shared_ptr<SceneNode> node);
    bool reorderNodeBefore(std::shared_ptr<SceneNode> dragged, std::shared_ptr<SceneNode> target);
    std::shared_ptr<SceneNode> findNodeShared(SceneNode* nodePtr) const;
    void selectAllChildren(std::shared_ptr<SceneNode> node);

    std::shared_ptr<SceneNode> instantiateNodeSubtree(std::shared_ptr<SceneNode> source,
                                                       std::shared_ptr<SceneNode> parent,
                                                       const std::string& rootNameSuffix = "_Copy");
    std::shared_ptr<SceneNode> instantiateTemplate(const std::string& filepath,
                                                   std::shared_ptr<SceneNode> parent);
    
    bool saveSceneToFile(const std::string& filepath);
    bool saveActiveScene();
    bool loadSceneFromFile(const std::string& filepath);
    const std::string& getActiveSceneFilePath() const { return activeSceneFilePath_; }
    void createNewScene();
    
    glm::vec2 getViewportSize() const { return viewportSize; }
    void setViewportSize(const glm::vec2& size);
    std::unique_ptr<Framebuffer>& getViewportFramebuffer() { return viewportFramebuffer; }

    BuildSystem& getBuildSystem() { return buildSystem; }

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

    bool isShowNavMeshDebugEnabled() const { return showNavMeshDebug; }
    void setShowNavMeshDebugEnabled(bool enabled) { showNavMeshDebug = enabled; }

private:
    void buildGridMesh();
    void renderGridInViewport(CameraComponent* camera);
    void compileSceneBinary(const std::string& jsonPath);
    std::shared_ptr<Scene> activeScene;
    std::string activeSceneFilePath_;
    std::weak_ptr<SceneNode> selectedNode;
    
    CameraMode cameraMode;
    std::shared_ptr<SceneNode> editorCamera;
    
    bool viewportFocused;
    bool viewportHovered;
    bool cameraFlyActive;
    float cameraYaw;
    float cameraPitch;

    std::unique_ptr<Framebuffer> viewportFramebuffer;
    glm::vec2 viewportSize;
    
    std::unique_ptr<EditorUI> ui;

    BuildSystem buildSystem;

    glm::vec3 gridOrigin;
    float gridCellSize;
    int gridSizeX;
    int gridSizeZ;
    bool gridLockEnabled;
    bool showGrid;
    bool gridMeshDirty;
    bool showNavMeshDebug;

    GLuint gridVao;
    GLuint gridVbo;
    GLuint gridIbo;
    GLsizei gridLineCount;
    std::shared_ptr<Shader> gridShader;

    struct ShadowCaster {
        std::shared_ptr<Mesh> mesh;
        glm::mat4 modelMatrix;
    };

    ShadowAtlasGL shadowAtlas;
    std::vector<ShadowCaster> shadowCasters;
    bool shadowAtlasReady;

    void createDefaultScene();
    void makeSubtreeNamesUnique(std::shared_ptr<SceneNode> node);
    void updateCameraFlyState();
    void beginCameraFly();
    void endCameraFly();
    glm::quat updateFlyRotation(const glm::vec2& mouseDelta);
    void handleViewportInput();
    void handleGameCameraInput(float deltaTime);
    void renderSceneToViewport();
    
    void renderSceneDirectly(Scene& scene, CameraComponent* camera);
    void renderNodeDirectly(std::shared_ptr<SceneNode> node, const glm::mat4& parentTransform, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, bool isEditorCamera, int renderPass);
    void renderSkyboxDirectly(Scene& scene, CameraComponent* camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

    void collectShadowCasters(std::shared_ptr<SceneNode> node, const glm::mat4& parentTransform);
    
    void renderShadowPassDirectly(CameraComponent* camera);
    
    void renderPhysicsDebugShapes(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void renderScreenSpaceTextDirectly(std::shared_ptr<SceneNode> node, bool isEditorCamera);
    void renderNavMeshDebug(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void syncNavGridFromScene();
    void processPendingNodeDeletions();
    void deleteNodeImmediate(std::shared_ptr<SceneNode> node);
    bool isInSubtree(std::shared_ptr<SceneNode> candidate, std::shared_ptr<SceneNode> subtreeRoot) const;
    void destroyComponentsPostOrder(std::shared_ptr<SceneNode> node);
    std::shared_ptr<SceneNode> findFirstCameraExcluding(std::shared_ptr<SceneNode> current,
                                                        std::shared_ptr<SceneNode> excludedSubtree) const;
    bool detachNodeFromScene(std::shared_ptr<SceneNode> node);
    bool detachNodeRecursive(std::shared_ptr<SceneNode> current, std::shared_ptr<SceneNode> target);

    std::vector<std::shared_ptr<SceneNode>> pendingNodeDeletions_;
};

} // namespace GameEngine

#endif // LINUX_BUILD
#endif // EDITOR_SYSTEM_H
