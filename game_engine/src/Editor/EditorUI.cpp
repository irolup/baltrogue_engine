#ifdef LINUX_BUILD

#include "Editor/EditorUI.h"
#include "Editor/EditorConsole.h"
#include "Editor/EditorTheme.h"
#include "Core/AssetPaths.h"
#include "Core/Ray.h"
#include "Scene/ScenePicker.h"
#include "Editor/EditorSystem.h"
#include "Core/MemoryProfiler.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/MaterialComponent.h"
#include "Components/LightComponent.h"
#include "Components/CameraComponent.h"
#include "Components/PhysicsComponent.h"
#include "Components/Area3DComponent.h"
#include "Components/RaycastComponent.h"
#include "Components/NavObstacleComponent.h"
#include "Components/NavAgentComponent.h"
#include "Components/NavVolumeComponent.h"
#include "Components/TextComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/SoundComponent.h"
#include "Components/SkyboxComponent.h"
#include "Components/BeamRenderer.h"
#include "Core/Engine.h"
#include "Core/Project.h"
#include "Editor/BuildSystem.h"
#include "Editor/SceneSerializer.h"
#include "Editor/FileDialog.h"
#include "Editor/LiveAreaBuilder.h"
#include "Editor/ProjectAssets.h"
#include "Editor/RecentProjects.h"
#include "Editor/NodeTemplateSerializer.h"
#include "Rendering/Framebuffer.h"
#include "Rendering/ShadowMap.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Texture.h"
#include "Physics/PhysicsManager.h"
#include "Input/InputMapping.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>

// ImGui includes
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

// ImGuizmo includes
#include "../../vendor/imguizmo/ImGuizmo.h"

namespace GameEngine {

EditorUI::EditorUI(EditorSystem& editorSystem)
    : editor(editorSystem)
    , browser(editorSystem)
    , showDemoWindow(false)
    , showSceneGraph(true)
    , showProperties(true)
    , showViewport(true)
    , showFileExplorer(true)
    , showInputMapping(false)
    , showMemoryViewer(false)
    , showBuildSettings(false)
    , showConsole(true)
    , showInfoLogs(true)
    , showWarningLogs(true)
    , showErrorLogs(true)
    , sceneGraphWidth(300.0f)
    , propertiesWidth(350.0f)
    , fileExplorerHeight(200.0f)
    , expandAllNodes(false)
    , collapseAllNodes(false)
{
}

void EditorUI::setupDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    
    ImGui::End();
}

bool EditorUI::rotationCacheMatches(const glm::quat& cached, const glm::quat& current) {
    return std::abs(glm::dot(cached, current)) > 1.0f - 1e-6f;
}

void EditorUI::syncRotationEditCache(SceneNode* node, const glm::quat& rotation) {
    rotationEditCacheNode = node;
    rotationEditCacheQuat = rotation;
    rotationEditCache = glm::degrees(glm::eulerAngles(rotation));
}

void EditorUI::attachNewNode(const std::shared_ptr<SceneNode>& parent, const std::shared_ptr<SceneNode>& newNode) {
    if (!parent || !newNode) {
        return;
    }
    parent->addChild(newNode);
    editor.selectNode(newNode);
}

void EditorUI::attachNewNodeToSelection(const std::shared_ptr<Scene>& scene, const std::shared_ptr<SceneNode>& newNode) {
    if (!scene || !newNode) {
        return;
    }
    auto parent = editor.getSelectedNode();
    if (!parent) {
        parent = scene->getRootNode();
    }
    attachNewNode(parent, newNode);
}

void EditorUI::openSceneFromDialog() {
    std::string filepath = FileDialog::openFileDialog("Open Scene", "*.json");
    if (!FileDialog::isValidResult(filepath)) {
        return;
    }
    openScenePath(filepath);
}

void EditorUI::openScenePath(const std::string& filepath) {
    if (!ProjectAssets::isInsideProject(filepath)) {
        EditorConsole::getInstance().logWarning(
            "Scene is outside the project: " + filepath +
            " - its asset references resolve against this project, and saving writes back outside it.");
    }

    if (editor.loadSceneFromFile(filepath)) {
        std::cout << "Scene loaded from: " << filepath << std::endl;
    } else {
        EditorConsole::getInstance().logError("Failed to load scene from: " + filepath);
    }
}

void EditorUI::openProjectFromDialog() {
    const std::string filepath = FileDialog::openProjectFileDialog("Open Project");
    if (!FileDialog::isValidResult(filepath)) {
        return;
    }
    editor.requestProjectRestart(filepath);
}

void EditorUI::renderProjectMenuItems() {
    Project& project = Project::getInstance();

    if (ImGui::MenuItem("New Project...")) {
        openNewProjectPopup = true;
        newProjectError.clear();
    }
    if (ImGui::MenuItem("Open Project...")) {
        openProjectFromDialog();
    }

    if (ImGui::BeginMenu("Open Recent Project")) {
        const std::vector<std::string> recent = RecentProjects::load();
        if (recent.empty()) {
            ImGui::TextDisabled("No recent projects");
        }
        for (const std::string& entry : recent) {
            const bool isCurrent = (entry == project.getFilePath());
            if (ImGui::MenuItem(entry.c_str(), nullptr, isCurrent, !isCurrent)) {
                editor.requestProjectRestart(entry);
            }
        }
        if (!recent.empty()) {
            ImGui::Separator();
            if (ImGui::MenuItem("Clear List")) {
                RecentProjects::clear();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Save Project", nullptr, false, project.isLoaded())) {
        if (project.save()) {
            EditorConsole::getInstance().logInfo("Saved " + project.getFilePath());
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Writes %s: build settings, main scene and recent scenes.", Project::kFileName);
    }
}

void EditorUI::renderNewProjectPopup() {
    if (openNewProjectPopup) {
        ImGui::OpenPopup("NewProject");
        openNewProjectPopup = false;
    }

    if (!ImGui::BeginPopupModal("NewProject", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Creates <folder>/%s and the asset folders a project needs.", Project::kFileName);
    ImGui::Separator();

    ImGui::InputText("Name", newProjectName, sizeof(newProjectName));
    ImGui::InputText("Folder", newProjectLocation, sizeof(newProjectLocation));
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        const std::string picked = FileDialog::openFolderDialog("New Project Folder");
        if (FileDialog::isValidResult(picked)) {
            std::snprintf(newProjectLocation, sizeof(newProjectLocation), "%s", picked.c_str());
            if (newProjectName[0] == '\0') {
                const std::string folderName = std::filesystem::path(picked).filename().string();
                std::snprintf(newProjectName, sizeof(newProjectName), "%s", folderName.c_str());
            }
        }
    }

    if (!newProjectError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", newProjectError.c_str());
    }

    ImGui::Separator();
    ImGui::TextDisabled("The editor restarts into the new project.");

    if (ImGui::Button("Create", ImVec2(120, 0))) {
        if (Project::create(newProjectLocation, newProjectName, newProjectError)) {
            const std::string created = (std::filesystem::path(newProjectLocation) / Project::kFileName).string();
            ImGui::CloseCurrentPopup();
            editor.requestProjectRestart(created);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        newProjectError.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorUI::saveSceneAsDialog() {
    if (!editor.getActiveScene()) {
        std::cout << "No active scene to save" << std::endl;
        return;
    }

    std::string filepath = FileDialog::saveFileDialog("Save Scene As", "*.json", "scene.json");
    if (!FileDialog::isValidResult(filepath)) {
        return;
    }
    if (editor.saveSceneToFile(filepath)) {
        std::cout << "Scene saved to: " << filepath << std::endl;
    } else {
        std::cout << "Failed to save scene to: " << filepath << std::endl;
    }
}

void EditorUI::saveActiveScene() {
    if (!editor.getActiveScene()) {
        std::cout << "No active scene to save" << std::endl;
        return;
    }

    if (editor.getActiveSceneFilePath().empty()) {
        saveSceneAsDialog();
        return;
    }

    if (editor.saveActiveScene()) {
        std::cout << "Scene saved to: " << editor.getActiveSceneFilePath() << std::endl;
    } else {
        std::cout << "Failed to save scene to: " << editor.getActiveSceneFilePath() << std::endl;
    }
}

void EditorUI::drawShadowSettingsForPlatform(const char* platform) {
    ShadowSettings& shadowSettings = ShadowManager::getInstance().getSettingsForPlatform(platform);

    // Both tabs use the same widget labels, so they need distinct ImGui ids
    ImGui::PushID(platform);

    ImGui::Checkbox("Enable Shadows", &shadowSettings.enabled);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Master switch. Off means the shadow pass never runs.");
    }

    const int tileSizes[] = {128, 256, 512, 1024};
    const char* tileSizeLabels[] = {"128 (Vita low)", "256 (Vita)", "512", "1024"};
    int currentTileSize = 1;
    for (int i = 0; i < 4; ++i) {
        if (tileSizes[i] == shadowSettings.tileSize) currentTileSize = i;
    }
    if (ImGui::Combo("Tile Resolution", &currentTileSize, tileSizeLabels, 4)) {
        shadowSettings.tileSize = tileSizes[currentTileSize];
    }

    ImGui::Checkbox("Soft Shadows (2x2 PCF)", &shadowSettings.softShadows);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Four taps instead of one. Leave off on Vita.");
    }

    ImGui::Separator();
    ImGui::Text("Directional light coverage");
    ImGui::DragFloat("Extent", &shadowSettings.directionalExtent, 0.5f, 2.0f, 200.0f, "%.1f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Half-size of the shadowed box around the camera. Smaller is sharper.");
    }
    ImGui::DragFloat("Depth", &shadowSettings.directionalDepth, 1.0f, 5.0f, 500.0f, "%.1f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How far back the light starts. Must reach every caster above the ground.");
    }

    if (ImGui::Button("Reset to Defaults")) {
        shadowSettings = ShadowManager::getDefaultSettings(platform);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Back to the stock values for this platform. Only this tab.");
    }

    ImGui::PopID();
}

void EditorUI::renderMenuBar() {
    static bool openGridSettingsNextFrame = false;
    static bool openPhysicsSettingsNextFrame = false;
    static bool openShadowSettingsNextFrame = false;

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
        openSceneFromDialog();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
        saveActiveScene();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
        saveSceneAsDialog();
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            renderProjectMenuItems();
            ImGui::Separator();

            if (ImGui::MenuItem("New Scene")) {
                editor.createNewScene();
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                openSceneFromDialog();
            }
            if (ImGui::BeginMenu("Open Recent Scene")) {
                const std::vector<std::string>& recentScenes = Project::getInstance().getRecentScenes();
                if (recentScenes.empty()) {
                    ImGui::TextDisabled("No recent scenes");
                }
                const std::vector<std::string> entries = recentScenes;
                for (const std::string& entry : entries) {
                    const bool isCurrent = (entry == editor.getActiveSceneFilePath());
                    if (ImGui::MenuItem(entry.c_str(), nullptr, isCurrent, !isCurrent)) {
                        openScenePath(entry);
                    }
                }
                if (!entries.empty()) {
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear List")) {
                        Project::getInstance().clearRecentScenes();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Scenes opened in this project. Saved with it.");
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                saveActiveScene();
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                saveSceneAsDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                GetEngine().setRunning(false);
            }
            ImGui::EndMenu();
        }
        renderNewProjectPopup();

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Graph", nullptr, &showSceneGraph);
            ImGui::MenuItem("Properties", nullptr, &showProperties);
            ImGui::MenuItem("Viewport", nullptr, &showViewport);
            ImGui::MenuItem("File Explorer", nullptr, &showFileExplorer);
            ImGui::MenuItem("Input Mapping", nullptr, &showInputMapping);
            ImGui::MenuItem("Project Settings", nullptr, &showBuildSettings);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("The project's name, main scene and asset root, plus per-platform build names: the Linux executable and window title, the VPK and its LiveArea images.");
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                const EditorTheme::Id current = EditorTheme::getCurrent();
                for (int i = 0; i < static_cast<int>(EditorTheme::Id::Count); ++i) {
                    const EditorTheme::Id id = static_cast<EditorTheme::Id>(i);
                    const char* label = EditorTheme::name(id);
                    if (!label) {
                        continue;
                    }
                    if (ImGui::MenuItem(label, nullptr, id == current)) {
                        EditorTheme::apply(id);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Editor colour scheme. Applies immediately; not saved between sessions yet.");
            }

            ImGui::Separator();
            ImGui::MenuItem("Demo Window", nullptr, &showDemoWindow);

            bool showNavMeshDebug = editor.isShowNavMeshDebugEnabled();
            if (ImGui::MenuItem("Nav Mesh Wireframe", nullptr, &showNavMeshDebug)) {
                editor.setShowNavMeshDebugEnabled(showNavMeshDebug);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Draw the navigation grid in the viewport (green = walkable, red = blocked).");
            }
            ImGui::Separator();
            bool gridLock = editor.isGridLockEnabled();
            if (ImGui::MenuItem("Grid Lock", nullptr, &gridLock)) {
                editor.setGridLockEnabled(gridLock);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("When enabled, node position snaps to the placement grid (X, Y, Z).");
            }
            bool showGrid = editor.isShowGridEnabled();
            if (ImGui::MenuItem("Show Grid", nullptr, &showGrid)) {
                editor.setShowGridEnabled(showGrid);
            }
            if (ImGui::MenuItem("Grid Settings...")) {
                openGridSettingsNextFrame = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Physics Settings...")) {
                openPhysicsSettingsNextFrame = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("World gravity and other physics settings.");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Shadow Settings...")) {
                openShadowSettingsNextFrame = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Shadow map resolution and quality. Enable casting per light in its inspector.");
            }

            ImGui::EndMenu();
        }
        if (openGridSettingsNextFrame) {
            ImGui::OpenPopup("GridSettings");
            openGridSettingsNextFrame = false;
        }
        if (openPhysicsSettingsNextFrame) {
            ImGui::OpenPopup("PhysicsSettings");
            openPhysicsSettingsNextFrame = false;
        }
        if (openShadowSettingsNextFrame) {
            ImGui::OpenPopup("ShadowSettings");
            openShadowSettingsNextFrame = false;
        }
        if (ImGui::BeginPopupModal("ShadowSettings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            auto& shadowManager = ShadowManager::getInstance();

            ImGui::Text("Atlas: %d x %d (%u tiles of %d)",
                        shadowManager.getAtlasWidth(), shadowManager.getAtlasHeight(),
                        kMaxShadowViews, shadowManager.getSettings().tileSize);
            ImGui::Text("Tiles in use this frame: %zu / %u",
                        shadowManager.getViewCount(), kMaxShadowViews);
            ImGui::TextDisabled("Saved to %s. Each build reads its own tab.", kShadowSettingsPath);

            if (ImGui::BeginTabBar("ShadowPlatforms")) {
                if (ImGui::BeginTabItem("PC")) {
                    drawShadowSettingsForPlatform("pc");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Vita")) {
                    drawShadowSettingsForPlatform("vita");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::Separator();
            if (ImGui::Button("Save")) {
                shadowManager.saveSettings();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Writes both tabs. Also saved automatically when the editor closes.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("PhysicsSettings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            glm::vec3 gravity = PhysicsManager::getInstance().getGravity();
            if (ImGui::DragFloat3("World Gravity", &gravity.x, 0.5f, -50.0f, 50.0f, "%.2f")) {
                PhysicsManager::getInstance().setGravity(gravity);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Default (0, -9.81, 0). Affects all dynamic bodies with gravity enabled.");
            }
            if (ImGui::Button("Reset (-9.81 Y)")) {
                PhysicsManager::getInstance().setGravity(glm::vec3(0.0f, -9.81f, 0.0f));
            }
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("GridSettings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            float cellSize = editor.getGridCellSize();
            if (ImGui::DragFloat("Cell Size", &cellSize, 0.1f, 0.1f, 20.0f, "%.2f")) {
                editor.setGridCellSize(cellSize);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("World units per grid cell (e.g. 1.0 = 1 unit).");
            }
            glm::vec3 origin = editor.getGridOrigin();
            if (ImGui::DragFloat3("Grid Origin", &origin.x, 0.25f)) {
                editor.setGridOrigin(origin);
            }
            ImGui::Spacing();
            ImGui::Text("Grid distance (cells) - min 1 x 1");
            int sx = editor.getGridSizeX();
            int sz = editor.getGridSizeZ();
            bool gridSizeChanged = false;
            if (ImGui::SliderInt("Distance X (cells)", &sx, 1, 128)) {
                sx = (sx < 1) ? 1 : sx;
                gridSizeChanged = true;
            }
            if (ImGui::SliderInt("Distance Z (cells)", &sz, 1, 128)) {
                sz = (sz < 1) ? 1 : sz;
                gridSizeChanged = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Number of cells; minimum 1x1 (grid would disappear at 0x0).");
            }
            if (gridSizeChanged) {
                editor.setGridSize(sx, sz);
            }
            ImGui::Spacing();
            ImGui::Text("Base tile bounds for future AI pathfinding.");
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty Node")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Node"));
                    attachNewNodeToSelection(scene, node);
                }
            }
            
            if (ImGui::MenuItem("Camera")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Camera"));
                    node->addComponent<CameraComponent>();
                    attachNewNode(scene->getRootNode(), node);
                    scene->setActiveCamera(node);
                }
            }
            
            if (ImGui::MenuItem("Text")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Text"));
                    auto textComponent = node->addComponent<TextComponent>();
                    textComponent->setText("Hello World!");
                    textComponent->setFontPath("assets/fonts/DroidSans.ttf");
                    textComponent->setFontSize(32.0f);
                    textComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                    textComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
                    textComponent->setAlignment(TextAlignment::CENTER);
                    textComponent->start();
                    
                    attachNewNodeToSelection(scene, node);
                }
            }
            
            if (ImGui::MenuItem("Area3D")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Area3D"));
                    auto area3DComponent = node->addComponent<Area3DComponent>();
                    area3DComponent->start();
                    
                    attachNewNodeToSelection(scene, node);
                }
            }
            
            if (ImGui::MenuItem("Sound")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Sound"));
                    auto soundComponent = node->addComponent<SoundComponent>();
                    soundComponent->start();
                    
                    attachNewNodeToSelection(scene, node);
                }
            }
            
            if (ImGui::BeginMenu("Mesh Shapes")) {
                if (ImGui::MenuItem("Plane")) { 
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("PlaneMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createPlane(1.0f, 1.0f, 1));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }

                if (ImGui::MenuItem("Cube")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CubeMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCube());
                        
                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Sphere")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("SphereMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createSphere(32, 16));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Capsule")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CapsuleMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCapsule(0.5f, 0.5f));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Cylinder")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CylinderMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCylinder(0.5f, 0.5f));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }

                if (ImGui::MenuItem("Ramp")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("RampMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createRamp());

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);

                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                ImGui::EndMenu();
            }
            
            if (ImGui::MenuItem("3D Model")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Model"));
                    node->addComponent<ModelRenderer>();
                    
                    attachNewNodeToSelection(scene, node);
                }
            }
            
            if (ImGui::BeginMenu("Collision Shapes")) {
                if (ImGui::MenuItem("Box Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("BoxCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Sphere Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("SphereCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Capsule Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CapsuleCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::CAPSULE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Cylinder Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CylinderCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::CYLINDER);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Plane Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("PlaneCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::PLANE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        attachNewNodeToSelection(scene, node);
                    }
                }

                if (ImGui::MenuItem("Ramp Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("RampCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::RAMP);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();

                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                if (ImGui::MenuItem("Raycast")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("Raycast"));
                        auto raycastComponent = node->addComponent<RaycastComponent>();
                        raycastComponent->setShowDebugLine(true);
                        attachNewNodeToSelection(scene, node);
                    }
                }
                
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Complete Entity")) {
                if (ImGui::MenuItem("Box Entity")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto parentNode = scene->createNode(editor.generateUniqueNodeName("BoxEntity"));
                        
                        auto meshNode = scene->createNode("Mesh");
                        auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCube());
                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                        auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        parentNode->addChild(collisionNode);
                        
                        attachNewNodeToSelection(scene, parentNode);
                    }
                }
                
                if (ImGui::MenuItem("Sphere Entity")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto parentNode = scene->createNode(editor.generateUniqueNodeName("SphereEntity"));
                        
                        auto meshNode = scene->createNode("Mesh");
                        auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createSphere(32, 16));
                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                        auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        parentNode->addChild(collisionNode);
                        
                        attachNewNodeToSelection(scene, parentNode);
                    }
                }
                
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Lights")) {
                if (ImGui::MenuItem("Point Light")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("PointLight"));
                        auto lightComponent = node->addComponent<LightComponent>();
                        lightComponent->setType(LightType::POINT);
                        lightComponent->setColor(glm::vec3(1.0f, 0.8f, 0.6f));
                        lightComponent->setIntensity(2.0f);
                        lightComponent->setRange(8.0f);
                        lightComponent->setShowGizmo(true);
                        node->getTransform().setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
                        
                        attachNewNodeToSelection(scene, node);
                        
                        lightComponent->start();
                    }
                }
                
                if (ImGui::MenuItem("Directional Light")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("DirectionalLight"));
                        auto lightComponent = node->addComponent<LightComponent>();
                        lightComponent->setType(LightType::DIRECTIONAL);
                        lightComponent->setColor(glm::vec3(0.8f, 0.9f, 0.6f)); // Cool white
                        lightComponent->setIntensity(1.5f);
                        lightComponent->setDirection(glm::vec3(-0.5f, -1.0f, -0.3f));
                        lightComponent->setShowGizmo(true);
                        node->getTransform().setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
                        
                        attachNewNodeToSelection(scene, node);
                        
                        lightComponent->start();
                    }
                }
                
                if (ImGui::MenuItem("Spot Light")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("SpotLight"));
                        auto lightComponent = node->addComponent<LightComponent>();
                        lightComponent->setType(LightType::SPOT);
                        lightComponent->setColor(glm::vec3(1.0f, 0.6f, 0.8f)); // Pink
                        lightComponent->setIntensity(3.0f);
                        lightComponent->setRange(12.0f);
                        lightComponent->setDirection(glm::vec3(0.0f, -1.0f, 0.0f));
                        lightComponent->setCutOff(glm::radians(25.0f));
                        lightComponent->setOuterCutOff(glm::radians(35.0f));
                        lightComponent->setShowGizmo(true);
                        node->getTransform().setPosition(glm::vec3(0.0f, 8.0f, 0.0f));
                        
                        attachNewNodeToSelection(scene, node);
                        
                        lightComponent->start();
                    }
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginPopupModal("No Node Selected", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Please select a node first to create a child.");
            if (ImGui::Button("OK")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginMenu("Build")) {
            BuildSystem& buildSystem = editor.getBuildSystem();
            const bool busy = buildSystem.isBuilding();

            if (ImGui::MenuItem("Build for Linux", nullptr, false, !busy)) {
                buildSystem.startBuild(BuildSystem::Target::Linux);
            }
            if (ImGui::MenuItem("Build VPK for PS Vita", nullptr, false, !busy)) {
                buildSystem.startBuild(BuildSystem::Target::Vita);
            }
            if (busy) {
                ImGui::Separator();
                ImGui::TextDisabled("Build running...");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Scene")) {
            const bool hasSceneFile = !editor.getActiveSceneFilePath().empty();
            if (ImGui::MenuItem("Set as Main Scene", nullptr, false, hasSceneFile)) {
                setActiveSceneAsMainScene();
            }
            if (ImGui::IsItemHovered() && !hasSceneFile) {
                ImGui::SetTooltip("Save the scene to a file first.");
            }

            if (ImGui::MenuItem("Update Asset Manifests")) {
                SceneSerializer::generateAssetManifests();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Rewrites the texture and input manifests the Vita VPK packs.");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Memory", nullptr, &showMemoryViewer);
            ImGui::MenuItem("Console", nullptr, &showConsole);
            ImGui::EndMenu();
        }

        renderPlayControls();

        ImGui::EndMainMenuBar();
    }
}

void EditorUI::renderSceneGraph() {
    ImGui::Begin("Scene Graph", &showSceneGraph);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        auto selected = editor.getSelectedNode();
        if (selected && ImGui::GetIO().KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && editor.canMoveNodeUp(selected)) {
                editor.moveNodeUp(selected);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && editor.canMoveNodeDown(selected)) {
                editor.moveNodeDown(selected);
            }
        }
    }
    
    auto scene = editor.getActiveScene();
    if (scene) {
        ImGui::Text("Scene: %s", scene->getName().c_str());
        ImGui::Separator();
        
        if (ImGui::Button("Expand All")) {
            expandAllNodes = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Collapse All")) {
            collapseAllNodes = true;
        }
        ImGui::SameLine();
        {
            auto selected = editor.getSelectedNode();
            const bool canFocus = selected && editor.canFocusCamera();
            ImGui::BeginDisabled(!canFocus);
            if (ImGui::Button("Focus on Selected")) {
                editor.focusCameraOnNode(selected);
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (!selected) {
                    ImGui::SetTooltip("Select a node first.");
                } else if (!editor.canFocusCamera()) {
                    ImGui::SetTooltip("Only available while the viewport uses the editor camera.");
                } else {
                    ImGui::SetTooltip("Move the editor camera to frame '%s'.", selected->getName().c_str());
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Insert Template...")) {
            std::string filepath = FileDialog::openTemplateFileDialog("Insert Template");
            if (FileDialog::isValidResult(filepath)) {
                auto parent = editor.getSelectedNode();
                if (!parent) {
                    parent = scene->getRootNode();
                }
                auto instance = editor.instantiateTemplate(filepath, parent);
                if (instance) {
                    editor.selectNode(instance);
                }
            }
        }

        // Notice a selection change from anywhere
        auto selectedNow = editor.getSelectedNode();
        SceneNode* selectedPtr = selectedNow.get();
        if (selectedPtr != lastSeenSelection) {
            lastSeenSelection = selectedPtr;
            revealSelectedNode = selectedPtr != nullptr;
            scrollToSelectedNode = revealSelectedNode && !selectionChangedFromTree;
        }
        selectionChangedFromTree = false;

        auto rootNode = scene->getRootNode();
        if (rootNode) {
            if (expandAllNodes || collapseAllNodes) {
                setSceneSubtreeOpen(rootNode, expandAllNodes);
            }
            if (revealSelectedNode && selectedPtr) {
                openScenePathToNode(rootNode, selectedPtr);
            }
            renderSceneNode(rootNode, 0);
        }

        expandAllNodes = false;
        collapseAllNodes = false;
        revealSelectedNode = false;
        scrollToSelectedNode = false;
    } else {
        ImGui::Text("No active scene");
    }
    
    ImGui::End();
}

void EditorUI::setSceneSubtreeOpen(const std::shared_ptr<SceneNode>& node, bool open) {
    if (!node) return;

    ImGui::PushID(node.get());
    const ImGuiID nodeId = ImGui::GetID((void*)(intptr_t)node.get());
    ImGui::TreeNodeSetOpen(nodeId, open);

    ImGui::PushOverrideID(nodeId);
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        setSceneSubtreeOpen(node->getChild(i), open);
    }
    ImGui::PopID();

    ImGui::PopID();
}

bool EditorUI::openScenePathToNode(const std::shared_ptr<SceneNode>& node, const SceneNode* target) {
    if (!node || !target) return false;

    ImGui::PushID(node.get());
    const ImGuiID nodeId = ImGui::GetID((void*)(intptr_t)node.get());

    bool found = (node.get() == target);
    if (!found) {
        ImGui::PushOverrideID(nodeId);
        for (size_t i = 0; i < node->getChildCount() && !found; ++i) {
            found = openScenePathToNode(node->getChild(i), target);
        }
        ImGui::PopID();

        if (found) {
            ImGui::TreeNodeSetOpen(nodeId, true);
        }
    }

    ImGui::PopID();
    return found;
}

void EditorUI::renderSceneNode(std::shared_ptr<SceneNode> node, int depth) {
    if (!node) return;
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    
    if (node == editor.getSelectedNode()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (node->getChildCount() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    
    std::string nodeLabel = node->getName();
    if (node->getComponent<ModelRenderer>()) {
        nodeLabel = "[MODEL] " + nodeLabel;
    } else if (node->getComponent<MeshRenderer>()) {
        nodeLabel = "[MESH] " + nodeLabel;
    } else if (node->getComponent<CameraComponent>()) {
        nodeLabel = "[CAM] " + nodeLabel;
    } else if (node->getComponent<LightComponent>()) {
        nodeLabel = "[LIGHT] " + nodeLabel;
    } else if (node->getComponent<PhysicsComponent>()) {
        nodeLabel = "[PHYS] " + nodeLabel; // Collision/physics node
    } else if (node->getChildCount() > 0) {
        nodeLabel = "[NODE] " + nodeLabel;
    } else {
        nodeLabel = "[OBJ] " + nodeLabel;
    }
    
    ImGui::PushID(node.get());

    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)node.get(), flags, "%s", nodeLabel.c_str());

    if (scrollToSelectedNode && node.get() == lastSeenSelection) {
        ImGui::SetScrollHereY(0.5f);
    }

    if (ImGui::IsItemClicked()) {
        selectionChangedFromTree = true;
        editor.selectNode(node);
    }

    const bool canReorder = editor.getActiveScene() && node != editor.getActiveScene()->getRootNode();
    if (canReorder && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        SceneNode* nodePtr = node.get();
        ImGui::SetDragDropPayload("SCENE_NODE", &nodePtr, sizeof(SceneNode*));
        ImGui::Text("Reorder %s", node->getName().c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
            IM_ASSERT(payload->DataSize == sizeof(SceneNode*));
            SceneNode* draggedPtr = *static_cast<SceneNode* const*>(payload->Data);
            if (auto dragged = editor.findNodeShared(draggedPtr)) {
                editor.reorderNodeBefore(dragged, node);
            }
        }

        const std::string droppedModel = ProjectAssets::acceptDrop(ProjectAssets::Kind::Model);
        if (!droppedModel.empty()) {
            if (auto targetScene = editor.getActiveScene()) {
                const std::string name = std::filesystem::path(droppedModel).stem().string();
                auto child = targetScene->createNode(editor.generateUniqueNodeName(name));
                auto renderer = child->addComponent<ModelRenderer>();
                if (!renderer->loadModel(droppedModel)) {
                    EditorConsole::getInstance().logError("Could not load " + droppedModel);
                }
                attachNewNode(node, child);
            }
        }

        const std::string droppedTemplate = ProjectAssets::acceptDrop(ProjectAssets::Kind::Template);
        if (!droppedTemplate.empty()) {
            editor.instantiateTemplate(droppedTemplate, node);
        }

        ImGui::EndDragDropTarget();
    }
    
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::BeginMenu("Create")) {
                if (ImGui::MenuItem("Empty Node")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Node"));
                        attachNewNode(node, child);
                    }
                }
                
                if (ImGui::MenuItem("Camera")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Camera"));
                        child->addComponent<CameraComponent>();
                        attachNewNode(node, child);
                    }
                }
                
                if (ImGui::MenuItem("Text")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Text"));
                        auto textComponent = child->addComponent<TextComponent>();
                        textComponent->setText("Hello World!");
                        textComponent->setFontPath("assets/fonts/DroidSans.ttf");
                        textComponent->setFontSize(32.0f);
                        textComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                        textComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
                        textComponent->setAlignment(TextAlignment::CENTER);
                        textComponent->start();
                        attachNewNode(node, child);
                    }
                }
                
                if (ImGui::MenuItem("Area3D")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Area3D"));
                        auto area3DComponent = child->addComponent<Area3DComponent>();
                        area3DComponent->start();
                        attachNewNode(node, child);
                    }
                }
                
                if (ImGui::MenuItem("Sound")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Sound"));
                        auto soundComponent = child->addComponent<SoundComponent>();
                        soundComponent->start();
                        attachNewNode(node, child);
                    }
                }
                
                if (ImGui::MenuItem("Skybox")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Skybox"));
                        auto skyboxComponent = child->addComponent<SkyboxComponent>();
                        // Default paths to skybox_1
                        skyboxComponent->setRightTexture("assets/textures/skyboxes/skybox_1/right.jpg");
                        skyboxComponent->setLeftTexture("assets/textures/skyboxes/skybox_1/left.jpg");
                        skyboxComponent->setTopTexture("assets/textures/skyboxes/skybox_1/top.jpg");
                        skyboxComponent->setBottomTexture("assets/textures/skyboxes/skybox_1/bottom.jpg");
                        skyboxComponent->setFrontTexture("assets/textures/skyboxes/skybox_1/front.jpg");
                        skyboxComponent->setBackTexture("assets/textures/skyboxes/skybox_1/back.jpg");
                        skyboxComponent->start();
                        scene->setActiveSkybox(child);
                        attachNewNode(node, child);
                    }
                }
                
                if (ImGui::BeginMenu("Mesh Shapes")) {
                    if (ImGui::MenuItem("Plane")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("PlaneMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createPlane(1.0f, 1.0f, 1));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                            meshRenderer->setMaterial(material);
                            
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Cube")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CubeMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCube());
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                            meshRenderer->setMaterial(material);
                            
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Sphere")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("SphereMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createSphere(32, 16));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                            meshRenderer->setMaterial(material);
                            
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Capsule")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CapsuleMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCapsule(0.5f, 0.5f));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                            meshRenderer->setMaterial(material);
                            
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Cylinder")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CylinderMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCylinder(0.5f, 0.5f));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                            meshRenderer->setMaterial(material);
                            
                            attachNewNode(node, child);
                        }
                    }

                    if (ImGui::MenuItem("Ramp")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("RampMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createRamp());

                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                            meshRenderer->setMaterial(material);

                            attachNewNode(node, child);
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                if (ImGui::BeginMenu("Complete Entity")) {
                    if (ImGui::MenuItem("Box Entity")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto parentNode = scene->createNode(editor.generateUniqueNodeName("BoxEntity"));
                            
                            auto meshNode = scene->createNode("Mesh");
                            auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCube());
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                            auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            parentNode->addChild(collisionNode);
                            
                            // Add parent to selected node or root
                            attachNewNodeToSelection(scene, parentNode);
                        }
                    }
                    
                    if (ImGui::MenuItem("Sphere Entity")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                        auto parentNode = scene->createNode(editor.generateUniqueNodeName("SphereEntity"));
                        
                        auto meshNode = scene->createNode("Mesh");
                            auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createSphere(32, 16));
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                            auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            parentNode->addChild(collisionNode);
                            
                            // Add parent to selected node or root
                            attachNewNodeToSelection(scene, parentNode);
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                // Lights submenu
                if (ImGui::BeginMenu("Lights")) {
                    if (ImGui::MenuItem("Point Light")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("PointLight"));
                            auto lightComponent = child->addComponent<LightComponent>();
                            lightComponent->setType(LightType::POINT);
                            lightComponent->setColor(glm::vec3(1.0f, 0.8f, 0.6f));
                            lightComponent->setIntensity(2.0f);
                            lightComponent->setRange(8.0f);
                            lightComponent->setShowGizmo(true);
                            child->getTransform().setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
                            attachNewNode(node, child);
                            lightComponent->start();
                        }
                    }
                    
                    if (ImGui::MenuItem("Directional Light")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("DirectionalLight"));
                            auto lightComponent = child->addComponent<LightComponent>();
                            lightComponent->setType(LightType::DIRECTIONAL);
                            lightComponent->setColor(glm::vec3(0.8f, 0.9f, 0.6f));
                            lightComponent->setIntensity(1.5f);
                            lightComponent->setDirection(glm::vec3(-0.5f, -1.0f, -0.3f));
                            lightComponent->setShowGizmo(true);
                            child->getTransform().setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
                            attachNewNode(node, child);
                            lightComponent->start();
                        }
                    }
                    
                    if (ImGui::MenuItem("Spot Light")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("SpotLight"));
                            auto lightComponent = child->addComponent<LightComponent>();
                            lightComponent->setType(LightType::SPOT);
                            lightComponent->setColor(glm::vec3(1.0f, 0.6f, 0.8f));
                            lightComponent->setIntensity(3.0f);
                            lightComponent->setRange(12.0f);
                            lightComponent->setDirection(glm::vec3(0.0f, -1.0f, 0.0f));
                            lightComponent->setCutOff(glm::radians(25.0f));
                            lightComponent->setOuterCutOff(glm::radians(35.0f));
                            lightComponent->setShowGizmo(true);
                            child->getTransform().setPosition(glm::vec3(0.0f, 8.0f, 0.0f));
                            attachNewNode(node, child);
                            lightComponent->start();
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                if (ImGui::MenuItem("3D Model")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Model"));
                        child->addComponent<ModelRenderer>();
                        attachNewNode(node, child);
                    }
                }
                
                if (ImGui::BeginMenu("Collision Shapes")) {
                    if (ImGui::MenuItem("Box Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("BoxCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Sphere Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("SphereCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Capsule Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CapsuleCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::CAPSULE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Cylinder Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CylinderCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::CYLINDER);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Plane Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("PlaneCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::PLANE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            attachNewNode(node, child);
                        }
                    }

                    if (ImGui::MenuItem("Ramp Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("RampCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::RAMP);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            attachNewNode(node, child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Raycast")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("Raycast"));
                            auto raycastComponent = child->addComponent<RaycastComponent>();
                            raycastComponent->setShowDebugLine(true);
                            attachNewNode(node, child);
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                ImGui::EndMenu();
            }
            
            ImGui::Separator();

            if (canReorder) {
                if (ImGui::MenuItem("Move Up", "Ctrl+Up", false, editor.canMoveNodeUp(node))) {
                    editor.moveNodeUp(node);
                }
                if (ImGui::MenuItem("Move Down", "Ctrl+Down", false, editor.canMoveNodeDown(node))) {
                    editor.moveNodeDown(node);
                }
                ImGui::Separator();
            }
            
            if (ImGui::MenuItem("Duplicate")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    std::shared_ptr<SceneNode> parent = scene->getRootNode();
                    if (node->getParent()) {
                        parent = editor.findNodeShared(node->getParent());
                    }
                    auto duplicate = editor.instantiateNodeSubtree(node, parent, "");
                    if (duplicate) {
                        editor.selectNode(duplicate);
                    }
                }
            }

            const bool isSceneRoot = editor.getActiveScene() && node == editor.getActiveScene()->getRootNode();
            if (!isSceneRoot && ImGui::MenuItem("Save as Template...")) {
                std::string defaultName = node->getName() + ".template.json";
                std::string filepath = FileDialog::saveTemplateFileDialog("Save as Template", defaultName);
                if (FileDialog::isValidResult(filepath)) {
                    NodeTemplateSerializer::saveNodeTemplate(node, filepath);
                }
            }

            if (ImGui::MenuItem("Insert Template...")) {
                std::string filepath = FileDialog::openTemplateFileDialog("Insert Template");
                if (FileDialog::isValidResult(filepath)) {
                    auto instance = editor.instantiateTemplate(filepath, node);
                    if (instance) {
                        editor.selectNode(instance);
                    }
                }
            }
            
            if (ImGui::MenuItem("Delete")) {
                editor.deleteNode(node);
            }
            ImGui::EndPopup();
        }
    
    if (nodeOpen) {
        for (size_t i = 0; i < node->getChildCount(); ++i) {
            renderSceneNode(node->getChild(i), depth + 1);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void EditorUI::renderProperties() {
    ImGui::Begin("Properties", &showProperties);
    
    auto selected = editor.getSelectedNode();
    if (selected) {
        static char nodeNameBuffer[256];
        strncpy(nodeNameBuffer, selected->getName().c_str(), sizeof(nodeNameBuffer) - 1);
        nodeNameBuffer[sizeof(nodeNameBuffer) - 1] = '\0';
        
        if (ImGui::InputText("Node Name", nodeNameBuffer, sizeof(nodeNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            selected->setName(nodeNameBuffer);
        }
        
        ImGui::Separator();
        
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& transform = selected->getTransform();
            
            auto physicsComp = selected->getComponent<PhysicsComponent>();
            if (physicsComp) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Yellow warning color
                ImGui::TextWrapped("WARNING: This node has a PhysicsComponent!");
                ImGui::TextWrapped("Moving this node directly can cause bugs. Consider moving the parent node instead.");
                ImGui::PopStyleColor();
                ImGui::Separator();
            }
            
            glm::vec3 position = transform.getPosition();
            if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
                transform.setPosition(position);
            }
            
            if (selected.get() != rotationEditCacheNode ||
                !rotationCacheMatches(rotationEditCacheQuat, transform.getRotation())) {
                syncRotationEditCache(selected.get(), transform.getRotation());
            }
            glm::vec3 rotation = rotationEditCache;
            if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) {
                rotationEditCache = rotation;
                transform.setEulerAngles(rotation);
                rotationEditCacheQuat = transform.getRotation();
            }
            
            glm::vec3 scale = transform.getScale();
            if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) {
                transform.setScale(scale);
            }
        }
        
        if (ImGui::CollapsingHeader("Node Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool visible = selected->isVisible();
            if (ImGui::Checkbox("Visible", &visible)) {
                selected->setVisible(visible);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Controls whether the node is rendered (drawn on screen).");
                ImGui::Separator();
                ImGui::Text("Checked: Node is visible and rendered");
                ImGui::Text("Unchecked: Node is hidden (not rendered)");
                ImGui::Separator();
                ImGui::Text("Note: Node can still update when hidden.");
                ImGui::Text("Use this to hide/show UI elements or objects.");
                ImGui::EndTooltip();
            }
            
            bool active = selected->isActive();
            if (ImGui::Checkbox("Active", &active)) {
                selected->setActive(active);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Controls whether the node updates AND renders.");
                ImGui::Separator();
                ImGui::Text("Checked: Node updates and renders (if also visible)");
                ImGui::Text("Unchecked: Node is frozen (no updates, no rendering)");
                ImGui::Separator();
                ImGui::Text("Combinations:");
                ImGui::BulletText("Visible=ON, Active=ON: Fully functional");
                ImGui::BulletText("Visible=OFF, Active=ON: Hidden but updating");
                ImGui::BulletText("Visible=ON, Active=OFF: Visible but frozen");
                ImGui::BulletText("Visible=OFF, Active=OFF: Completely disabled");
                ImGui::EndTooltip();
            }
        }
        
        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Add Component")) {
                ImGui::OpenPopup("AddComponentPopup");
            }
            
            if (ImGui::BeginPopup("AddComponentPopup")) {
                if (ImGui::MenuItem("Camera Component")) {
                    if (!selected->getComponent<CameraComponent>()) {
                        selected->addComponent<CameraComponent>();
                    }
                }
                if (ImGui::MenuItem("Mesh Renderer")) {
                    if (!selected->getComponent<MeshRenderer>()) {
                        selected->addComponent<MeshRenderer>();
                    }
                }
                if (ImGui::MenuItem("Model Renderer")) {
                    if (!selected->getComponent<ModelRenderer>()) {
                        selected->addComponent<ModelRenderer>();
                    }
                }
                if (ImGui::MenuItem("Material Overrides")) {
                    if (!selected->getComponent<MaterialComponent>()) {
                        selected->addComponent<MaterialComponent>();
                    }
                }
                if (ImGui::MenuItem("Animation Component")) {
                    if (!selected->getComponent<AnimationComponent>()) {
                        auto animComponent = selected->addComponent<AnimationComponent>();
                        animComponent->start(); // Initialize the animation component
                    }
                }
                if (ImGui::MenuItem("Light Component")) {
                    if (!selected->getComponent<LightComponent>()) {
                        selected->addComponent<LightComponent>();
                    }
                }
                if (ImGui::MenuItem("Physics Component")) {
                    if (!selected->getComponent<PhysicsComponent>()) {
                        selected->addComponent<PhysicsComponent>();
                        // Warning is shown in Transform section when PhysicsComponent is detected
                    }
                }
                if (ImGui::MenuItem("Area3D Component")) {
                    if (!selected->getComponent<Area3DComponent>()) {
                        auto area3DComponent = selected->addComponent<Area3DComponent>();
                        area3DComponent->start(); // Initialize the area3D component
                    }
                }
                if (ImGui::MenuItem("Text Component")) {
                    if (!selected->getComponent<TextComponent>()) {
                        auto textComponent = selected->addComponent<TextComponent>();
                        textComponent->setText("Hello World!");
                        textComponent->setFontPath("assets/fonts/DroidSans.ttf");
                        textComponent->setFontSize(32.0f);
                        textComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                        textComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
                        textComponent->setAlignment(TextAlignment::CENTER);
                        textComponent->start(); // Initialize the text component
                    }
                }
                if (ImGui::MenuItem("Script Component")) {
                    if (!selected->getComponent<ScriptComponent>()) {
                        auto scriptComponent = selected->addComponent<ScriptComponent>();
                        scriptComponent->start();
                    }
                }
                if (ImGui::MenuItem("Sound Component")) {
                    if (!selected->getComponent<SoundComponent>()) {
                        auto soundComponent = selected->addComponent<SoundComponent>();
                        soundComponent->start();
                    }
                }
                if (ImGui::MenuItem("Skybox Component")) {
                    if (!selected->getComponent<SkyboxComponent>()) {
                        auto scene = editor.getActiveScene();
                        auto skyboxComponent = selected->addComponent<SkyboxComponent>();
                        // Set default paths to skybox_1
                        skyboxComponent->setRightTexture("assets/textures/skyboxes/skybox_1/right.jpg");
                        skyboxComponent->setLeftTexture("assets/textures/skyboxes/skybox_1/left.jpg");
                        skyboxComponent->setTopTexture("assets/textures/skyboxes/skybox_1/top.jpg");
                        skyboxComponent->setBottomTexture("assets/textures/skyboxes/skybox_1/bottom.jpg");
                        skyboxComponent->setFrontTexture("assets/textures/skyboxes/skybox_1/front.jpg");
                        skyboxComponent->setBackTexture("assets/textures/skyboxes/skybox_1/back.jpg");
                        skyboxComponent->start();
                        if (scene) {
                            scene->setActiveSkybox(selected);
                        }
                    }
                }
                if (ImGui::MenuItem("Nav Obstacle")) {
                    if (!selected->getComponent<NavObstacleComponent>()) {
                        auto comp = selected->addComponent<NavObstacleComponent>();
                        comp->start();
                    }
                }
                if (ImGui::MenuItem("Nav Agent")) {
                    if (!selected->getComponent<NavAgentComponent>()) {
                        selected->addComponent<NavAgentComponent>();
                    }
                }
                if (ImGui::MenuItem("Nav Volume")) {
                    if (!selected->getComponent<NavVolumeComponent>()) {
                        auto comp = selected->addComponent<NavVolumeComponent>();
                        comp->start();
                    }
                }
                ImGui::EndPopup();
            }
            
            ImGui::Separator();
            
            const auto& components = selected->getAllComponents();
            std::string componentToRemove;
            
            for (size_t i = 0; i < components.size(); i++) {
                const auto& component = components[i];
                if (component && component->isEnabled()) {
                    ImGui::PushID(static_cast<int>(i));
                    
                    std::string componentName = component->getTypeName();
                    std::string popupId = "ComponentContextMenu_" + std::to_string(i);
                    
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;
                    bool treeOpen = ImGui::TreeNodeEx(componentName.c_str(), flags);
                    
                    if (ImGui::BeginPopupContextItem(popupId.c_str())) {
                        if (ImGui::MenuItem("Remove Component")) {
                            componentToRemove = componentName;
                        }
                        ImGui::EndPopup();
                    }
                    
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                    if (ImGui::SmallButton("X")) {
                        componentToRemove = componentName;
                    }
                    ImGui::PopStyleColor(3);
                    
                    if (treeOpen) {
                        component->drawInspector();
                        ImGui::TreePop();
                    }
                    
                    ImGui::PopID();
                }
            }
            
            if (!componentToRemove.empty()) {
                if (componentToRemove == "CameraComponent") {
                    selected->removeComponent<CameraComponent>();
                } else if (componentToRemove == "MeshRenderer") {
                    selected->removeComponent<MeshRenderer>();
                } else if (componentToRemove == "ModelRenderer") {
                    selected->removeComponent<ModelRenderer>();
                } else if (componentToRemove == "MaterialComponent") {
                    selected->removeComponent<MaterialComponent>();
                } else if (componentToRemove == "AnimationComponent") {
                    selected->removeComponent<AnimationComponent>();
                } else if (componentToRemove == "LightComponent") {
                    selected->removeComponent<LightComponent>();
                } else if (componentToRemove == "PhysicsComponent") {
                    selected->removeComponent<PhysicsComponent>();
                } else if (componentToRemove == "Area3DComponent") {
                    selected->removeComponent<Area3DComponent>();
                } else if (componentToRemove == "TextComponent") {
                    selected->removeComponent<TextComponent>();
                } else if (componentToRemove == "ScriptComponent") {
                    selected->removeComponent<ScriptComponent>();
                } else if (componentToRemove == "SoundComponent") {
                    selected->removeComponent<SoundComponent>();
                } else if (componentToRemove == "SkyboxComponent") {
                    selected->removeComponent<SkyboxComponent>();
                } else if (componentToRemove == "NavAgentComponent") {
                    selected->removeComponent<NavAgentComponent>();
                } else if (componentToRemove == "NavObstacleComponent") {
                    selected->removeComponent<NavObstacleComponent>();
                } else if (componentToRemove == "NavVolumeComponent") {
                    selected->removeComponent<NavVolumeComponent>();
                } else if (componentToRemove == "RaycastComponent") {
                    selected->removeComponent<RaycastComponent>();
                } else if (componentToRemove == "BeamRenderer") {
                    selected->removeComponent<BeamRenderer>();
                }
            }
            
            // Show collision shape info if this is a collision-only node
            if (selected->getComponent<PhysicsComponent>() && !selected->getComponent<MeshRenderer>()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "⚡ Collision Shape Node");
                ImGui::TextWrapped("This node contains only collision physics. It will be invisible in the game but will participate in physics simulation.");
                ImGui::TextWrapped("You can move and scale this node to adjust the collision shape independently from the visual mesh.");
            }
            
            // Model Renderer properties
            auto modelRenderer = selected->getComponent<ModelRenderer>();
            if (modelRenderer) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "🎨 Model Renderer");
                
                // Model file path
                ImGui::Text("Model Path:");
                ImGui::SameLine();
                if (modelRenderer->isModelLoaded()) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", modelRenderer->getModelPath().c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "No model loaded");
                }
                
                if (ImGui::Button("Load Model...")) {
                    const std::string picked = FileDialog::openFileDialog("Select 3D Model", "*.gltf *.glb");
                    if (FileDialog::isValidResult(picked)) {
                        std::string importError;
                        const std::string modelPath =
                            ProjectAssets::importIntoProject(picked, importError);
                        if (modelPath.empty()) {
                            EditorConsole::getInstance().logError(importError);
                        } else if (modelRenderer->loadModel(modelPath)) {
                            EditorConsole::getInstance().logInfo("Loaded model: " + modelPath);
                        } else {
                            EditorConsole::getInstance().logError("Failed to load model: " + modelPath);
                        }
                    }
                }
                if (ImGui::BeginDragDropTarget()) {
                    const std::string dropped = ProjectAssets::acceptDrop(ProjectAssets::Kind::Model);
                    if (!dropped.empty() && !modelRenderer->loadModel(dropped)) {
                        EditorConsole::getInstance().logError("Failed to load model: " + dropped);
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Or drag a model here from the File Explorer.");
                }
                
                if (modelRenderer->isModelLoaded()) {
                    ImGui::Separator();
                    ImGui::Text("Model Info:");
                    
                    auto meshes = modelRenderer->getMeshes();
                    auto materials = modelRenderer->getMaterials();
                    
                    ImGui::Text("Meshes: %zu", meshes.size());
                    ImGui::Text("Materials: %zu", materials.size());
                    
                    if (!materials.empty()) {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "📁 Loaded Textures:");
                        
                        for (size_t i = 0; i < materials.size(); ++i) {
                            auto material = materials[i];
                            if (material) {
                                ImGui::PushID(static_cast<int>(i));
                                
                                if (ImGui::CollapsingHeader(("Material " + std::to_string(i)).c_str())) {
                                    auto diffuseTexture = material->getDiffuseTexture();
                                    if (diffuseTexture) {
                                        ImGui::Text("Diffuse: %s", diffuseTexture->getFilePath().c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Diffuse: None");
                                    }
                                    
                                    // Normal texture
                                    auto normalTexture = material->getNormalTexture();
                                    if (normalTexture) {
                                        ImGui::Text("Normal: %s", normalTexture->getFilePath().c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Normal: None");
                                    }
                                    
                                    auto armTexture = material->getARMTexture();
                                    if (armTexture) {
                                        ImGui::Text("ARM: %s", armTexture->getFilePath().c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "ARM: None");
                                    }

                                    auto environmentTexture = material->getEnvironmentTexture();
                                    if (environmentTexture) {
                                        ImGui::Text("Environment: %s", environmentTexture->getFilePath().c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Environment: None");
                                    }
                                }
                                
                                ImGui::PopID();
                            }
                        }
                    }
                }
            }
        }
        
    } else {
        ImGui::Text("No node selected");
    }
    
    ImGui::End();
}

void EditorUI::renderViewport() {
    ImGui::Begin("Viewport", &showViewport);
    
    bool isViewportFocused = ImGui::IsWindowFocused();
    editor.setViewportFocused(isViewportFocused);

    editor.setViewportHovered(false);
    
    static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentMode = ImGuizmo::LOCAL;
    
    auto cameraNode = editor.getActiveCamera();
    auto selectedNode = editor.getSelectedNode();
    
    renderCameraControls();

    if (cameraNode && selectedNode) {
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, currentOperation == ImGuizmo::TRANSLATE ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Translate")) {
            currentOperation = ImGuizmo::TRANSLATE;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, currentOperation == ImGuizmo::ROTATE ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Rotate")) {
            currentOperation = ImGuizmo::ROTATE;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, currentOperation == ImGuizmo::SCALE ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Scale")) {
            currentOperation = ImGuizmo::SCALE;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    ImGui::Separator();

    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    
    const float VITA_ASPECT_RATIO = 960.0f / 544.0f;
    
    ImVec2 viewportSize;
    if (availableSize.x / availableSize.y > VITA_ASPECT_RATIO) {
        viewportSize.y = availableSize.y;
        viewportSize.x = availableSize.y * VITA_ASPECT_RATIO;
    } else {
        viewportSize.x = availableSize.x;
        viewportSize.y = availableSize.x / VITA_ASPECT_RATIO;
    }
    
    ImVec2 viewportPos = ImGui::GetCursorPos();
    viewportPos.x += (availableSize.x - viewportSize.x) * 0.5f;
    viewportPos.y += (availableSize.y - viewportSize.y) * 0.5f;
    ImGui::SetCursorPos(viewportPos);
    
    if (viewportSize.x != editor.getViewportSize().x || viewportSize.y != editor.getViewportSize().y) {
        editor.setViewportSize(glm::vec2(viewportSize.x, viewportSize.y));
        
        auto& framebuffer = editor.getViewportFramebuffer();
        if (framebuffer) {
            framebuffer->resize((int)viewportSize.x, (int)viewportSize.y);
        }
    }
    
    auto& framebuffer = editor.getViewportFramebuffer();
    if (!framebuffer && viewportSize.x > 0 && viewportSize.y > 0) {
        framebuffer = std::unique_ptr<Framebuffer>(new Framebuffer());
        if (!framebuffer->create((int)viewportSize.x, (int)viewportSize.y)) {
            std::cerr << "Failed to create viewport framebuffer!" << std::endl;
            framebuffer.reset();
        }
    }
    
    auto scene = editor.getActiveScene();
    if (scene && viewportSize.x > 0 && viewportSize.y > 0) {
        editor.renderSceneToViewport();
        
        ImGui::SetCursorPos(viewportPos);
        
        if (framebuffer) {
            ImGui::Image(
                (void*)(intptr_t)framebuffer->getColorTexture(),
                viewportSize,
                ImVec2(0, 1),
                ImVec2(1, 0)
            );
        }
        
        editor.setViewportHovered(ImGui::IsItemHovered());

        if (ImGui::BeginDragDropTarget()) {
            const std::string droppedModel = ProjectAssets::acceptDrop(ProjectAssets::Kind::Model);
            if (!droppedModel.empty()) {
                if (auto targetScene = editor.getActiveScene()) {
                    const std::string name =
                        std::filesystem::path(droppedModel).stem().string();
                    auto node = targetScene->createNode(editor.generateUniqueNodeName(name));
                    auto renderer = node->addComponent<ModelRenderer>();
                    if (!renderer->loadModel(droppedModel)) {
                        EditorConsole::getInstance().logError("Could not load " + droppedModel);
                    }
                    node->getTransform().setPosition(editor.getGridOrigin());
                    attachNewNode(targetScene->getRootNode(), node);
                }
            }

            const std::string droppedTemplate = ProjectAssets::acceptDrop(ProjectAssets::Kind::Template);
            if (!droppedTemplate.empty()) {
                if (auto targetScene = editor.getActiveScene()) {
                    editor.instantiateTemplate(droppedTemplate, targetScene->getRootNode());
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImVec2 imageScreenPos = ImGui::GetItemRectMin();
        ImVec2 imageScreenSize = ImVec2(ImGui::GetItemRectMax().x - imageScreenPos.x, ImGui::GetItemRectMax().y - imageScreenPos.y);
        
        if (!cameraNode) {
            ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + 30));
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No active camera");
            ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + 50));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Create a camera to view the scene");
        } else {
            if (selectedNode) {
                auto cameraComponent = cameraNode->getComponent<CameraComponent>();
                if (cameraComponent) {
                    ImGuizmo::Enable(!editor.isCameraFlyActive());
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                    ImGuizmo::SetRect(imageScreenPos.x, imageScreenPos.y, imageScreenSize.x, imageScreenSize.y);
                    
                    glm::mat4 viewMatrix = cameraComponent->getViewMatrix();
                    glm::mat4 projectionMatrix = cameraComponent->getProjectionMatrix();
                    
                    glm::mat4 worldMatrix = selectedNode->getWorldMatrix();
                    
                    float view[16];
                    float projection[16];
                    float matrix[16];
                    
                    memcpy(view, glm::value_ptr(viewMatrix), 16 * sizeof(float));
                    memcpy(projection, glm::value_ptr(projectionMatrix), 16 * sizeof(float));
                    memcpy(matrix, glm::value_ptr(worldMatrix), 16 * sizeof(float));
                    
                    bool manipulated = ImGuizmo::Manipulate(
                        view, projection,
                        currentOperation, currentMode,
                        matrix
                    );
                    
                    if (manipulated && ImGuizmo::IsUsing()) {
                        glm::mat4 newWorldMatrix;
                        memcpy(glm::value_ptr(newWorldMatrix), matrix, 16 * sizeof(float));
                        glm::vec3 worldPos(newWorldMatrix[3][0], newWorldMatrix[3][1], newWorldMatrix[3][2]);
                        if (editor.isGridLockEnabled()) {
                            worldPos = editor.snapWorldToGrid(worldPos, true);
                            newWorldMatrix[3][0] = worldPos.x;
                            newWorldMatrix[3][1] = worldPos.y;
                            newWorldMatrix[3][2] = worldPos.z;
                        }
                        auto parent = selectedNode->getParent();

                        glm::mat4 localMatrix = parent ? (glm::inverse(parent->getWorldMatrix()) * newWorldMatrix) : newWorldMatrix;
                        glm::vec3 newPosition(localMatrix[3]);
                        glm::vec3 newScale(
                            glm::length(glm::vec3(localMatrix[0])),
                            glm::length(glm::vec3(localMatrix[1])),
                            glm::length(glm::vec3(localMatrix[2]))
                        );
                        glm::mat3 rotationMatrix(
                            glm::vec3(localMatrix[0]) / newScale.x,
                            glm::vec3(localMatrix[1]) / newScale.y,
                            glm::vec3(localMatrix[2]) / newScale.z
                        );
                        glm::quat newRotation = glm::quat_cast(rotationMatrix);
                        selectedNode->getTransform().setPosition(newPosition);
                        selectedNode->getTransform().setRotation(newRotation);
                        selectedNode->getTransform().setScale(newScale);
                        syncRotationEditCache(selectedNode.get(), newRotation);
                    }
                }
            }

            const bool gizmoOwnsClick = selectedNode && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !gizmoOwnsClick && !editor.isCameraFlyActive()) {
                if (auto* cameraComponent = cameraNode->getComponent<CameraComponent>()) {
                    const ImVec2 mousePos = ImGui::GetMousePos();
                    const glm::vec2 viewportPoint(mousePos.x - imageScreenPos.x, mousePos.y - imageScreenPos.y);
                    const glm::vec2 imageSize(imageScreenSize.x, imageScreenSize.y);

                    const Ray ray = cameraComponent->screenPointToRay(viewportPoint, imageSize);
                    const PickHit pick = ScenePicker::pickNode(*scene, ray);

                    editor.selectNode(pick.hit ? pick.node : nullptr);
                }
            }
        }
        
        ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + viewportSize.y - 40));
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Vita Aspect Ratio (16:9)");
        ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + viewportSize.y - 20));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Viewport: %dx%d", (int)viewportSize.x, (int)viewportSize.y);
    } else {
        ImGui::Text("Viewport (%dx%d)", (int)viewportSize.x, (int)viewportSize.y);
        ImGui::Text("No active scene or invalid viewport size");
    }
    
    ImGui::End();
}

void EditorUI::renderPlayControls() {
    BuildSystem& buildSystem = editor.getBuildSystem();

    const float controlsWidth = 260.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - controlsWidth);

    if (buildSystem.isGameRunning()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Stop")) {
            buildSystem.stopGame();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Running");
        return;
    }

    if (buildSystem.isBuilding()) {
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "Building...");
        return;
    }

    const bool hasExecutable = buildSystem.gameExecutableExists();

    if (ImGui::Button("Play") && hasExecutable) {
        saveSceneBeforeLaunch();
        buildSystem.startGame();
    }
    if (!hasExecutable && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("No executable yet — use Build & Play");
    }

    ImGui::SameLine();
    if (ImGui::Button("Build & Play")) {
        saveSceneBeforeLaunch();
        buildSystem.buildAndStartGame();
    }

    if (hasExecutable && buildSystem.isGameExecutableStale()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "(stale)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sources changed since the last build");
        }
    }
}

void EditorUI::setActiveSceneAsMainScene() {
    const std::string scenePath = editor.getActiveSceneFilePath();
    if (scenePath.empty()) {
        return;
    }

    Project& project = Project::getInstance();
    project.mainScene = AssetPaths::toPortable(scenePath);

    if (project.save()) {
        EditorConsole::getInstance().logInfo("Main scene set to " + project.mainScene);
    } else {
        EditorConsole::getInstance().logError("Could not write " + std::string(Project::kFileName));
    }
}

void EditorUI::saveSceneBeforeLaunch() {
    auto scene = editor.getActiveScene();
    if (!scene || editor.getActiveSceneFilePath().empty()) {
        return;
    }

    if (editor.saveActiveScene()) {
        EditorConsole::getInstance().logInfo("Saved scene: " + editor.getActiveSceneFilePath());
    } else {
        EditorConsole::getInstance().logWarning("Could not save the active scene before launching");
    }
}

void EditorUI::renderConsole() {
    if (!showConsole) {
        return;
    }

    if (!ImGui::Begin("Console", &showConsole)) {
        ImGui::End();
        return;
    }

    EditorConsole& console = EditorConsole::getInstance();

    if (ImGui::Button("Clear")) {
        console.clear();
    }
    ImGui::SameLine();

    const bool copyAll = ImGui::Button("Copy");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copy every line currently shown (the severity filters apply).\n"
                          "Right-click a line to copy just that one.");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfoLogs);
    ImGui::SameLine();
    ImGui::Checkbox("Warnings", &showWarningLogs);
    ImGui::SameLine();
    ImGui::Checkbox("Errors", &showErrorLogs);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu warnings, %zu errors",
                        console.getCount(LogSeverity::Warning),
                        console.getCount(LogSeverity::Error));

    ImGui::Separator();

    ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    std::string copyBuffer;

    int entryIndex = 0;
    for (const EditorConsole::Entry& entry : console.getEntries()) {
        ImVec4 color(0.85f, 0.85f, 0.85f, 1.0f);
        switch (entry.severity) {
            case LogSeverity::Warning:
                if (!showWarningLogs) continue;
                color = ImVec4(0.95f, 0.85f, 0.35f, 1.0f);
                break;
            case LogSeverity::Error:
                if (!showErrorLogs) continue;
                color = ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
                break;
            case LogSeverity::Info:
            default:
                if (!showInfoLogs) continue;
                break;
        }

        if (copyAll) {
            copyBuffer += entry.message;
            copyBuffer += '\n';
        }

        ImGui::PushID(entryIndex++);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(entry.message.c_str());
        ImGui::PopStyleColor();

        if (ImGui::BeginPopupContextItem("LineContext")) {
            if (ImGui::MenuItem("Copy line")) {
                ImGui::SetClipboardText(entry.message.c_str());
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (copyAll) {
        ImGui::SetClipboardText(copyBuffer.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

void EditorUI::renderFileExplorer() {
    if (!showFileExplorer) {
        return;
    }
    browser.draw(&showFileExplorer);
}

void EditorUI::renderInputMapping() {
    if (!showInputMapping) return;
    
    ImGui::Begin("Input Mapping Configuration", &showInputMapping);
    
    auto& inputManager = GetEngine().getInputManager();
    auto& inputMapping = inputManager.getInputMapping();
    
    // Header with hot-reload status
    ImGui::Text("Input Action Configuration");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(Hot-reload enabled)");
    ImGui::Separator();
    
    // File operations section
    ImGui::Text("File Operations:");
    if (ImGui::Button("Reload from File")) {
        inputMapping.reloadMappings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save to File")) {
        inputMapping.saveMappingsToFile(Project::getInstance().inputMap);
        // Reload mappings to refresh the UI
        inputMapping.reloadMappings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open File in Editor")) {
#ifdef LINUX_BUILD
        const std::string command = "xdg-open '" + AssetPaths::resolve(Project::getInstance().inputMap) + "' &";
        int result = system(command.c_str());
        (void)result; // Ignore return value - we don't need to check if xdg-open succeeded
#endif
    }
    
    ImGui::Separator();
    
    // Add new action section
    ImGui::Text("Add New Action:");
    static char newActionName[64] = "";
    ImGui::InputText("Action Name", newActionName, sizeof(newActionName));
    
    static int selectedPlatform = 0;
    const char* platformNames[] = {"PC", "VITA", "ALL"};
    ImGui::Combo("Platform", &selectedPlatform, platformNames, 3);
    
    static int selectedInputType = 0;
    const char* inputTypeNames[] = {"Keyboard Key", "Vita Button", "Analog Stick", "Mouse Button", "Mouse Axis"};
    ImGui::Combo("Input Type", &selectedInputType, inputTypeNames, 5);
    
    static int inputCode = 0;
    ImGui::InputInt("Input Code", &inputCode);
    
    static int selectedActionType = 1;
    const char* actionTypeNames[] = {"Pressed", "Held", "Released"};
    ImGui::Combo("Action Type", &selectedActionType, actionTypeNames, 3);
    
    if (ImGui::Button("Add Action")) {
        if (strlen(newActionName) > 0) {
            InputMapping mapping(newActionName, 
                               static_cast<InputType>(selectedInputType), 
                               inputCode, 
                               static_cast<InputActionType>(selectedActionType));
            inputMapping.addMapping(mapping);
            
            newActionName[0] = '\0';
            inputCode = 0;
        }
    }
    
    ImGui::Separator();
    
    const auto& mappings = inputMapping.getAllMappings();
    
    if (mappings.empty()) {
        ImGui::Text("No input mappings configured.");
        ImGui::Text("Use the 'Add Action' section above to create new mappings.");
    } else {
        ImGui::Text("Current Mappings (%zu total):", mappings.size());
        
        ImGui::Columns(6, "InputMappingColumns");
        ImGui::SetColumnWidth(0, 120);
        ImGui::SetColumnWidth(1, 80);
        ImGui::SetColumnWidth(2, 100);
        ImGui::SetColumnWidth(3, 80);
        ImGui::SetColumnWidth(4, 80);
        ImGui::SetColumnWidth(5, 100);
        
        ImGui::Text("Action Name"); ImGui::NextColumn();
        ImGui::Text("Platform"); ImGui::NextColumn();
        ImGui::Text("Input Type"); ImGui::NextColumn();
        ImGui::Text("Code"); ImGui::NextColumn();
        ImGui::Text("Action"); ImGui::NextColumn();
        ImGui::Text("Controls"); ImGui::NextColumn();
        ImGui::Separator();
        
        for (size_t i = 0; i < mappings.size(); ++i) {
            const auto& mapping = mappings[i];
            
            // Action name
            ImGui::Text("%s", mapping.actionName.c_str());
            ImGui::NextColumn();
            
            // Platform (inferred from input type)
            const char* platform = "Unknown";
            if (mapping.inputType == InputType::KEYBOARD_KEY || 
                mapping.inputType == InputType::MOUSE_BUTTON || 
                mapping.inputType == InputType::MOUSE_AXIS) {
                platform = "PC";
            } else if (mapping.inputType == InputType::VITA_BUTTON) {
                platform = "VITA";
            } else if (mapping.inputType == InputType::ANALOG_STICK) {
                platform = "Both";
            }
            ImGui::Text("%s", platform);
            ImGui::NextColumn();
            
            const char* typeNames[] = {"Keyboard", "Vita Button", "Analog Stick", "Mouse Button", "Mouse Axis"};
            ImGui::Text("%s", typeNames[static_cast<int>(mapping.inputType)]);
            ImGui::NextColumn();
            
            if (mapping.inputType == InputType::KEYBOARD_KEY) {
                const char* keyName = "Unknown";
#ifdef LINUX_BUILD
                switch (mapping.inputCode) {
                    case GLFW_KEY_W: keyName = "W"; break;
                    case GLFW_KEY_A: keyName = "A"; break;
                    case GLFW_KEY_S: keyName = "S"; break;
                    case GLFW_KEY_D: keyName = "D"; break;
                    case GLFW_KEY_SPACE: keyName = "Space"; break;
                    case GLFW_KEY_LEFT_SHIFT: keyName = "Shift"; break;
                    case GLFW_KEY_LEFT_CONTROL: keyName = "Ctrl"; break;
                    case GLFW_KEY_E: keyName = "E"; break;
                    case GLFW_KEY_ESCAPE: keyName = "Esc"; break;
                    default: keyName = "Key"; break;
                }
#endif
                ImGui::Text("%s", keyName);
            } else {
                ImGui::Text("%d", mapping.inputCode);
            }
            ImGui::NextColumn();
            
            const char* actionNames[] = {"Pressed", "Held", "Released"};
            ImGui::Text("%s", actionNames[static_cast<int>(mapping.actionType)]);
            ImGui::NextColumn();
            
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Button("Test")) {
                bool isActive = inputMapping.isActionHeld(mapping.actionName);
                if (isActive) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ACTIVE");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                inputMapping.removeMapping(mapping.actionName);
            }
            ImGui::PopID();
            ImGui::NextColumn();
        }
        
        ImGui::Columns(1);
    }
    
    ImGui::Separator();
    
    // Preset operations
    ImGui::Text("Preset Operations:");
    if (ImGui::Button("Load Default Mappings")) {
        inputMapping.loadMappingsFromFile(Project::getInstance().defaultInputMap, true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Mappings")) {
        inputMapping.clearMappings();
    }
    
    ImGui::Separator();
    
    // Help section
    ImGui::Text("Help:");
    ImGui::BulletText("Hot-reload is enabled - changes to %s are automatically loaded", Project::getInstance().inputMap.c_str());
    ImGui::BulletText("Use 'Open File in Editor' to edit mappings in a text editor");
    ImGui::BulletText("Input codes: W=87, A=65, S=83, D=68, Space=32, Shift=340, Ctrl=341, E=69, Esc=256");
    ImGui::BulletText("Vita buttons: Cross=16384, Circle=32768, Square=8192, Triangle=4096");
    
    ImGui::End();
}

void EditorUI::renderMemoryViewer() {
    if (!showMemoryViewer) return;
    ImGui::Begin("Memory", &showMemoryViewer);
    ImGui::Separator();
    auto scene = editor.getActiveScene();
    auto entries = MemoryProfiler::getSummary(scene.get());
    size_t totalBytes = 0;
    for (const auto& e : entries) totalBytes += e.bytes;
    if (entries.empty()) {
        ImGui::TextUnformatted("No data (no scene or no resources).");
    } else {
        if (ImGui::BeginTable("MemoryTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();
            for (const auto& e : entries) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(e.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%zu", e.bytes);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zu", e.count);
                ImGui::TableSetColumnIndex(3);
                float pct = totalBytes > 0 ? (100.0f * static_cast<float>(e.bytes) / static_cast<float>(totalBytes)) : 0.0f;
                ImGui::Text("%.1f%%", pct);
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::Text("Total tracked: %zu bytes (%.2f MB)", totalBytes, totalBytes / (1024.0 * 1024.0));
    }
    ImGui::End();
}

void EditorUI::drawLiveAreaImageSlot(const char* label, const char* requirement, char* path, size_t pathSize) {
    ImGui::PushID(label);

    ImGui::InputText(label, path, pathSize);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        const std::string picked = FileDialog::openImageFileDialog(label);
        if (FileDialog::isValidResult(picked)) {
            // The Vita build tracks these as make prerequisites, so an absolute path outside the project would break the build on any other machine...
            std::string importError;
            const std::string imported = ProjectAssets::importIntoProject(picked, importError);
            if (imported.empty()) {
                EditorConsole::getInstance().logError(importError);
            } else {
                std::snprintf(path, pathSize, "%s", imported.c_str());
            }
        }
    }
    if (ImGui::BeginDragDropTarget()) {
        const std::string dropped = ProjectAssets::acceptDrop(ProjectAssets::Kind::Texture);
        if (!dropped.empty()) {
            std::snprintf(path, pathSize, "%s", dropped.c_str());
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        path[0] = '\0';
    }

    ImGui::TextDisabled("    %s", requirement);
    if (path[0] != '\0' && !AssetPaths::exists(AssetPaths::resolve(path))) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "- file not found");
    }

    ImGui::PopID();
}

void EditorUI::renderBuildSettings() {
    if (!showBuildSettings) return;

    static bool loaded = false;
    static bool toolsAvailable = false;
    static char projectName[64] = "";
    static char assetRoot[128] = "";
    static char inputMap[256] = "";
    static char pcTitle[64] = "";
    static char pcExecutableName[64] = "";
    static char mainScene[256] = "";
    static int windowWidth = 960;
    static int windowHeight = 544;
    static bool fullscreen = false;
    static int targetFrameRate = 60;
    static int rendererIndex = 0;
    static char vitaTitle[64] = "";
    static char vitaTitleId[16] = "";
    static char vitaAppVersion[8] = "";
    static char vpkName[64] = "";
    static char icon0[256] = "";
    static char pic0[256] = "";
    static char bg0[256] = "";
    static char startup[256] = "";
    static int styleIndex = 0;
    static std::string status;

    Project& project = Project::getInstance();

    if (!loaded) {
        std::snprintf(projectName, sizeof(projectName), "%s", project.name.c_str());
        std::snprintf(assetRoot, sizeof(assetRoot), "%s", project.assetRoot.c_str());
        std::snprintf(inputMap, sizeof(inputMap), "%s", project.inputMap.c_str());
        std::snprintf(pcTitle, sizeof(pcTitle), "%s", project.pc.title.c_str());
        std::snprintf(pcExecutableName, sizeof(pcExecutableName), "%s", project.pc.executableName.c_str());
        std::snprintf(mainScene, sizeof(mainScene), "%s", project.mainScene.c_str());
        windowWidth = project.pc.windowWidth;
        windowHeight = project.pc.windowHeight;
        fullscreen = project.pc.fullscreen;
        targetFrameRate = project.pc.targetFrameRate;
        rendererIndex = (project.pc.renderer == "opengl") ? 1 : 0;
        std::snprintf(vitaTitle, sizeof(vitaTitle), "%s", project.vita.title.c_str());
        std::snprintf(vitaTitleId, sizeof(vitaTitleId), "%s", project.vita.titleId.c_str());
        std::snprintf(vitaAppVersion, sizeof(vitaAppVersion), "%s", project.vita.appVersion.c_str());
        std::snprintf(vpkName, sizeof(vpkName), "%s", project.vita.vpkName.c_str());
        std::snprintf(icon0, sizeof(icon0), "%s", project.vita.icon0Source.c_str());
        std::snprintf(pic0, sizeof(pic0), "%s", project.vita.pic0Source.c_str());
        std::snprintf(bg0, sizeof(bg0), "%s", project.vita.bg0Source.c_str());
        std::snprintf(startup, sizeof(startup), "%s", project.vita.startupSource.c_str());
        styleIndex = (project.vita.liveAreaStyle == "psmobile") ? 1 : 0;

        toolsAvailable = LiveAreaBuilder::converterToolsAvailable();
        loaded = true;
    }

    ImGui::Begin("Project Settings", &showBuildSettings);

    if (project.isLoaded()) {
        ImGui::TextDisabled("Saved to %s. Each build reads its own tab.", project.getFilePath().c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "No %s yet - saving writes one next to the editor's working directory.", Project::kFileName);
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("BuildPlatforms")) {
        if (ImGui::BeginTabItem("Project")) {
            ImGui::InputText("Project Name", projectName, sizeof(projectName));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Shown in the editor title bar and the recent projects list.");
            }

            ImGui::InputText("Main Scene", mainScene, sizeof(mainScene));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Scene both builds boot into. Read at startup, no rebuild needed.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse##mainScene")) {
                const std::string picked = FileDialog::openFileDialog("Select Main Scene");
                if (FileDialog::isValidResult(picked)) {
                    if (ProjectAssets::isInsideProject(picked)) {
                        std::snprintf(mainScene, sizeof(mainScene), "%s", AssetPaths::toPortable(picked).c_str());
                    } else {
                        EditorConsole::getInstance().logError( "Main scene must be inside the project: " + picked);
                    }
                }
            }
            if (ImGui::BeginDragDropTarget()) {
                const std::string dropped = ProjectAssets::acceptDrop(ProjectAssets::Kind::Scene);
                if (!dropped.empty()) {
                    std::snprintf(mainScene, sizeof(mainScene), "%s", dropped.c_str());
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::TextDisabled("    the Vita reads the same value from the packed project file");

            ImGui::Separator();
            ImGui::Text("Folders");

            ImGui::InputText("Asset Root", assetRoot, sizeof(assetRoot));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Project-relative folder every asset path is looked up under. Takes effect on the next start.");
            }
            ImGui::InputText("Input Map", inputMap, sizeof(inputMap));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Where the editor and both builds read and write key bindings.");
            }

            ImGui::Separator();
            ImGui::TextDisabled("Project root: %s",
                                project.getRootPath().empty() ? "(working directory)" : project.getRootPath().c_str());
            ImGui::TextDisabled("Engine version: %s", Project::kEngineVersion);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("PC")) {
            ImGui::InputText("Game Name", pcTitle, sizeof(pcTitle));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Window title the Linux game sets at startup. Read at run time, no rebuild needed.");
            }
            ImGui::InputText("Executable Name", pcExecutableName, sizeof(pcExecutableName));
            ImGui::TextDisabled("    builds %s/%s",
                                (rendererIndex == 1) ? "build_linux_gl" : "build_linux", pcExecutableName);

            ImGui::Separator();
            ImGui::Text("Window");

            ImGui::InputInt("Width", &windowWidth);
            ImGui::InputInt("Height", &windowHeight);
            if (windowWidth < 1) windowWidth = 1;
            if (windowHeight < 1) windowHeight = 1;
            if (ImGui::Button("Match Vita (960 x 544)")) {
                windowWidth = 960;
                windowHeight = 544;
            }
            ImGui::Checkbox("Fullscreen", &fullscreen);
            ImGui::TextDisabled("    PC only; the Vita panel is always 960 x 544. The window stays resizable.");

            ImGui::Separator();
            ImGui::Text("Runtime");

            ImGui::InputInt("Target FPS", &targetFrameRate);
            if (targetFrameRate < 0) targetFrameRate = 0;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("0 leaves the frame rate unlimited.");
            }

            const char* rendererLabels[] = {"Vulkan", "OpenGL"};
            ImGui::Combo("Renderer", &rendererIndex, rendererLabels, 2);
            ImGui::TextDisabled("    a build flag, not a runtime switch: changing it rebuilds with make USE_VULKAN=%d",
                                (rendererIndex == 1) ? 0 : 1);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Vita")) {
            ImGui::InputText("Title", vitaTitle, sizeof(vitaTitle));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Name under the icon on the Vita home screen (TITLE in param.sfo).");
            }
            ImGui::InputText("Title ID", vitaTitleId, sizeof(vitaTitleId), ImGuiInputTextFlags_CharsUppercase);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Four letters then five digits, e.g. VSDK00420. Two apps sharing one id overwrite each other.");
            }
            ImGui::InputText("App Version", vitaAppVersion, sizeof(vitaAppVersion));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Shown on the LiveArea, format 01.00.");
            }
            ImGui::InputText("VPK Name", vpkName, sizeof(vpkName));
            ImGui::TextDisabled("    builds build/%s.vpk", vpkName);

            ImGui::Separator();
            ImGui::Text("LiveArea");

            const char* styleLabels[] = {"a1 (gate centred)", "psmobile (gate on the right)"};
            ImGui::Combo("Style", &styleIndex, styleLabels, 2);

            drawLiveAreaImageSlot("Icon (icon0.png)", "128 x 128, drawn as a rounded square, no transparency", icon0, sizeof(icon0));
            drawLiveAreaImageSlot("Loading Screen (pic0.png)", "960 x 544, reduced to a 256 colour palette", pic0, sizeof(pic0));
            drawLiveAreaImageSlot("Background (bg0.png)", "840 x 500, the paper behind the gate", bg0, sizeof(bg0));
            drawLiveAreaImageSlot("Gate Image (startup.png)", "280 x 158, above the Start button, alpha kept", startup, sizeof(startup));

            ImGui::TextDisabled("Sources are scaled and quantised into sce_sys/ on build. An empty slot ships without that image.");
            if (!toolsAvailable) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                                   "ffmpeg and pngquant are needed to convert the images: sudo apt install ffmpeg pngquant");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    bool saveRequested = false;
    bool generateRequested = false;
    if (ImGui::Button("Save")) {
        saveRequested = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Writes both tabs.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save and Generate LiveArea Assets")) {
        saveRequested = true;
        generateRequested = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Runs %s now. make vita runs it too, so this is only to preview the result.", kLiveAreaScriptPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        loaded = false;
        status.clear();
    }

    if (saveRequested) {
        Project edited = project;
        edited.name = projectName;
        edited.assetRoot = assetRoot;
        edited.inputMap = inputMap;
        edited.mainScene = mainScene;
        edited.pc.title = pcTitle;
        edited.pc.executableName = pcExecutableName;
        edited.pc.windowWidth = windowWidth;
        edited.pc.windowHeight = windowHeight;
        edited.pc.fullscreen = fullscreen;
        edited.pc.targetFrameRate = targetFrameRate;
        edited.pc.renderer = (rendererIndex == 1) ? "opengl" : "vulkan";
        edited.vita.title = vitaTitle;
        edited.vita.titleId = vitaTitleId;
        edited.vita.appVersion = vitaAppVersion;
        edited.vita.vpkName = vpkName;
        edited.vita.liveAreaStyle = (styleIndex == 1) ? "psmobile" : "a1";
        edited.vita.icon0Source = icon0;
        edited.vita.pic0Source = pic0;
        edited.vita.bg0Source = bg0;
        edited.vita.startupSource = startup;

        const std::string error = edited.validate();
        if (!error.empty()) {
            status = "Not saved: " + error;
        } else {
            project = edited;
            const bool written = project.isLoaded()
                ? project.save()
                : project.saveAs((std::filesystem::current_path() / Project::kFileName).string());

            if (!written) {
                status = "Could not write " + std::string(Project::kFileName);
            } else {
                status = "Saved to " + project.getFilePath();
                editor.updateWindowTitle();
                if (generateRequested) {
                    std::string output;
                    const bool generated = LiveAreaBuilder::generateAssets(output);
                    status = (generated ? "Wrote sce_sys/\n" : "Generating sce_sys/ failed\n") + output;
                }
            }
        }
    }

    if (!status.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());
    }

    ImGui::End();
}

void EditorUI::renderCameraControls() {
    // Safety check - only render if we have a valid scene
    if (!editor.getActiveScene()) {
        return;
    }
    
    auto& editorSystem = editor;
    auto currentMode = editorSystem.getCameraMode();
    const char* modeLabel = (currentMode == EditorSystem::CameraMode::EDITOR_CAMERA)
                          ? "Camera: Editor" : "Camera: Game";
    if (ImGui::Button(modeLabel)) {
        ImGui::OpenPopup("CameraControlsPopup");
    }

    if (editorSystem.isCameraFlyActive()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Look active");
    }

    ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
    if (!ImGui::BeginPopup("CameraControlsPopup")) {
        return;
    }

    // Camera mode selection
    ImGui::Text("Camera Mode:");

    if (ImGui::RadioButton("Editor Camera", currentMode == EditorSystem::CameraMode::EDITOR_CAMERA)) {
        editorSystem.setCameraMode(EditorSystem::CameraMode::EDITOR_CAMERA);
    }
    if (ImGui::RadioButton("Game Camera", currentMode == EditorSystem::CameraMode::GAME_CAMERA)) {
        editorSystem.setCameraMode(EditorSystem::CameraMode::GAME_CAMERA);
    }
    
    ImGui::Separator();
    
    // Show current camera info
    auto activeCamera = editorSystem.getActiveCamera();
    if (activeCamera) {
        ImGui::Text("Active: %s", activeCamera->getName().c_str());
        
        auto cameraComponent = activeCamera->getComponent<CameraComponent>();
        if (cameraComponent) {
            auto& transform = activeCamera->getTransform();
            auto pos = transform.getPosition();
            ImGui::Text("Position: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
            ImGui::Text("FOV: %.1f°", cameraComponent->getFOV());
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No camera available");
    }
    
    ImGui::Separator();
    
    // Show controls info
    if (currentMode == EditorSystem::CameraMode::EDITOR_CAMERA) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Editor Camera Controls:");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Game Camera Controls:");
    }
    ImGui::Text("Hold Right Mouse - Look");
    ImGui::Text("WASD - Move");
    ImGui::Text("Space/Shift - Up/Down");

    ImGui::EndPopup();
}

} // namespace GameEngine

#endif // LINUX_BUILD
