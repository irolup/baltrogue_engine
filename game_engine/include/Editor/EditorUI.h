#ifndef EDITOR_UI_H
#define EDITOR_UI_H

#ifdef LINUX_BUILD

#include <memory>
#include <string>
#include <glm/glm.hpp>
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

private:
    // For selection when creating a new node
    void attachNewNode(const std::shared_ptr<SceneNode>& parent, const std::shared_ptr<SceneNode>& newNode);
    void attachNewNodeToSelection(const std::shared_ptr<Scene>& scene, const std::shared_ptr<SceneNode>& newNode);

    // The buttons for launching the game in the editor
    void renderPlayControls();.
    void saveSceneBeforeLaunch();
    void setActiveSceneAsMainScene();

    void openSceneFromDialog();
    void saveSceneAsDialog();
    void saveActiveScene();
    void drawShadowSettingsForPlatform(const char* platform);
    void drawLiveAreaImageSlot(const char* label, const char* requirement, char* path, size_t pathSize);

    EditorSystem& editor;
    
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

    float sceneGraphWidth;
    float propertiesWidth;
    float fileExplorerHeight;
    
    bool expandAllNodes;
    bool collapseAllNodes;
    bool focusOnSelectedNode;
};

}

#endif
#endif
