#ifndef EDITOR_UI_H
#define EDITOR_UI_H

#ifdef LINUX_BUILD

#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Editor/ProjectBrowser.h"
namespace GameEngine {
class EditorSystem;
class Scene;
class SceneNode;
}

namespace GameEngine {

class EditorUI {
public:
    EditorUI(EditorSystem& editorSystem);
    ~EditorUI() = default;

    void setupDockspace();
    void renderMenuBar();
    void renderSceneGraph();
    void renderProperties();
    void renderViewport();
    void renderFileExplorer();
    void renderCameraControls();
    void renderInputMapping();
    void renderMemoryViewer();
    void renderBuildSettings();
    void renderConsole();
    void renderSceneNode(std::shared_ptr<SceneNode> node, int depth = 0);

    void setSceneSubtreeOpen(const std::shared_ptr<SceneNode>& node, bool open);

    bool openScenePathToNode(const std::shared_ptr<SceneNode>& node, const SceneNode* target);

private:
    // For selection when creating a new node
    void attachNewNode(const std::shared_ptr<SceneNode>& parent, const std::shared_ptr<SceneNode>& newNode);
    void attachNewNodeToSelection(const std::shared_ptr<Scene>& scene, const std::shared_ptr<SceneNode>& newNode);

    // The buttons for launching the game in the editor
    void renderPlayControls();
    void saveSceneBeforeLaunch();
    void setActiveSceneAsMainScene();

    void openSceneFromDialog();
    void openScenePath(const std::string& filepath);
    void saveSceneAsDialog();
    void saveActiveScene();
    void drawShadowSettingsForPlatform(const char* platform);
    void drawLiveAreaImageSlot(const char* label, const char* requirement, char* path, size_t pathSize);

    void renderProjectMenuItems();
    void renderNewProjectPopup();
    void openProjectFromDialog();

    EditorSystem& editor;

    ProjectBrowser browser;

    bool showDemoWindow;
    bool showSceneGraph;
    bool showProperties;
    bool showViewport;
    bool showFileExplorer;
    bool showInputMapping;
    bool showMemoryViewer;
    bool showBuildSettings;
    bool showConsole;
    bool showInfoLogs;
    bool showWarningLogs;
    bool showErrorLogs;

    bool openNewProjectPopup = false;
    char newProjectName[64] = "";
    char newProjectLocation[512] = "";
    std::string newProjectError;

    float sceneGraphWidth;
    float propertiesWidth;
    float fileExplorerHeight;
    
    bool expandAllNodes;
    bool collapseAllNodes;

    SceneNode* lastSeenSelection = nullptr;
    bool revealSelectedNode = false;
    bool scrollToSelectedNode = false;
    bool selectionChangedFromTree = false;

    SceneNode* rotationEditCacheNode = nullptr;
    glm::quat rotationEditCacheQuat{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 rotationEditCache{0.0f};

    static bool rotationCacheMatches(const glm::quat& cached, const glm::quat& current);

    void syncRotationEditCache(SceneNode* node, const glm::quat& rotation);
};

}

#endif
#endif
