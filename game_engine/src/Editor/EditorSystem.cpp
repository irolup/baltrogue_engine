#ifdef LINUX_BUILD

#include "Editor/EditorSystem.h"
#include "Editor/EditorConsole.h"
#include "Editor/EditorTheme.h"
#include "Editor/EditorUI.h"
#include "Editor/ProjectAssets.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Scene/SceneBinaryFormat.h"
#include "Scene/ScenePicker.h"
#include <imgui.h>
#include "../../vendor/imguizmo/ImGuizmo.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/LightComponent.h"
#include "Components/TextComponent.h"
#include "Components/Area3DComponent.h"
#include "Components/PhysicsComponent.h"
#include "Components/RaycastComponent.h"
#include "Components/SkyboxComponent.h"
#include "Components/NavVolumeComponent.h"
#include "Core/AssetPaths.h"
#include "Core/Engine.h"
#include "Core/Project.h"
#include "Rendering/LightingManager.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/Renderer.h"
#include "Rendering/Texture.h"
#include "Rendering/Mesh.h"
#include "Rendering/ShadowMap.h"
#include "Physics/PhysicsManager.h"
#include "Navigation/NavGrid.h"
#include "Navigation/NavGridRegistry.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>
#include <glm/gtc/quaternion.hpp>

namespace GameEngine {

EditorSystem::EditorSystem()
    : activeScene(nullptr)
    , cameraMode(CameraMode::EDITOR_CAMERA)
    , editorCamera(nullptr)
    , viewportFocused(false)
    , viewportHovered(false)
    , cameraFlyActive(false)
    , cameraYaw(0.0f)
    , cameraPitch(0.0f)
    , gridOrigin(0.0f, 0.0f, 0.0f)
    , gridCellSize(1.0f)
    , gridSizeX(64)
    , gridSizeZ(64)
    , gridLockEnabled(false)
    , showGrid(false)
    , gridMeshDirty(true)
    , showNavMeshDebug(false)
    , gridVao(0)
    , gridVbo(0)
    , gridIbo(0)
    , gridLineCount(0)
    , shadowAtlasReady(false)
{
    ui = std::unique_ptr<EditorUI>(new EditorUI(*this));
}

EditorSystem::~EditorSystem() {
    shutdown();
}

bool EditorSystem::initialize() {
    EditorConsole::getInstance().installStreamCapture();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = "config/imgui.ini";
    
    EditorTheme::apply(EditorTheme::getCurrent());

    GLFWwindow* window = glfwGetCurrentContext();
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
        return false;
    }

    glfwSetDropCallback(window, &EditorSystem::handleFileDrop);

    createDefaultScene();
    
    return true;
}

void EditorSystem::handleFileDrop(GLFWwindow* window, int count, const char** paths) {
    (void)window;

    for (int i = 0; i < count; ++i) {
        std::string error;
        const std::string imported = ProjectAssets::importIntoProject(paths[i], error);
        if (imported.empty()) {
            EditorConsole::getInstance().logError(error);
        } else {
            EditorConsole::getInstance().logInfo("Imported " + imported);
        }
    }
}

void EditorSystem::shutdown() {
    EditorConsole::getInstance().removeStreamCapture();

    if (gridVao) {
        glDeleteVertexArrays(1, &gridVao);
        gridVao = 0;
    }
    if (gridVbo) {
        glDeleteBuffers(1, &gridVbo);
        gridVbo = 0;
    }
    if (gridIbo) {
        glDeleteBuffers(1, &gridIbo);
        gridIbo = 0;
    }
    gridShader.reset();
    gridLineCount = 0;

    ShadowManager::getInstance().saveSettings();

    Project& project = Project::getInstance();
    if (project.isLoaded()) {
        project.save();
    }

    shadowAtlas.destroy();
    shadowCasters.clear();
    shadowAtlasReady = false;
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void EditorSystem::update(float deltaTime) {
    buildSystem.update();

    updateCameraFlyState();

    if (!cameraFlyActive) {
        return;
    }

    if (cameraMode == CameraMode::EDITOR_CAMERA && editorCamera) {
        handleViewportInput();
    } else if (cameraMode == CameraMode::GAME_CAMERA) {
        handleGameCameraInput(deltaTime);
    }
}

void EditorSystem::updateCameraFlyState() {
    auto& inputManager = GetEngine().getInputManager();

    if (!cameraFlyActive) {
        const bool startedInViewport = viewportHovered &&
                                       inputManager.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
        if (startedInViewport && !ImGui::GetIO().WantTextInput) {
            beginCameraFly();
        }
        return;
    }

    if (!inputManager.isMouseButtonHeld(GLFW_MOUSE_BUTTON_RIGHT)) {
        endCameraFly();
    }
}

void EditorSystem::beginCameraFly() {
    auto camera = getActiveCamera();
    if (camera) {
        glm::vec3 forward = camera->getTransform().getRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
        cameraPitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
        cameraYaw = std::atan2(-forward.x, -forward.z);
    }

    cameraFlyActive = true;
    GetEngine().getInputManager().setMouseCapture(true);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void EditorSystem::endCameraFly() {
    cameraFlyActive = false;
    GetEngine().getInputManager().setMouseCapture(false);
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

glm::quat EditorSystem::updateFlyRotation(const glm::vec2& mouseDelta) {
    const float lookSensitivity = 0.0025f; // radians per pixel
    cameraYaw -= mouseDelta.x * lookSensitivity;
    cameraPitch -= mouseDelta.y * lookSensitivity;

    const float limit = glm::half_pi<float>() - 0.01f;
    cameraPitch = glm::clamp(cameraPitch, -limit, limit);

    return glm::angleAxis(cameraYaw, glm::vec3(0, 1, 0)) *
           glm::angleAxis(cameraPitch, glm::vec3(1, 0, 0));
}

void EditorSystem::render() {
    processPendingNodeDeletions();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGuizmo::BeginFrame();
    
    ui->setupDockspace();
    ui->renderMenuBar();
    ui->renderSceneGraph();
    ui->renderProperties();
    ui->renderViewport();
    ui->renderFileExplorer();
    ui->renderInputMapping();
    ui->renderMemoryViewer();
    ui->renderBuildSettings();
    ui->renderConsole();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void EditorSystem::setActiveScene(std::shared_ptr<Scene> scene) {
    editorCamera = nullptr;
    
    activeScene = scene;
    clearSelection();
    
    if (activeScene) {
        editorCamera = std::make_shared<SceneNode>("Editor Camera");
        auto editorCameraComponent = editorCamera->addComponent<CameraComponent>();
        editorCameraComponent->setFOV(45.0f);
        editorCameraComponent->setActive(false);
        
        editorCamera->getTransform().setPosition(glm::vec3(0, 0, 5));
        editorCamera->getTransform().setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }
}

void EditorSystem::selectNode(std::shared_ptr<SceneNode> node) {
    auto prevSelected = selectedNode.lock();
    if (prevSelected) {
        prevSelected->setSelected(false);
    }
    
    selectedNode = node;
    if (node) {
        node->setSelected(true);
        if (activeScene) {
            activeScene->setSelectedNode(node);
        }
    }
}

void EditorSystem::accumulateWorldBounds(const std::shared_ptr<SceneNode>& node, glm::vec3& outMin, glm::vec3& outMax, bool& hasBounds) {
    if (!node) {
        return;
    }

    glm::vec3 localMin(0.0f);
    glm::vec3 localMax(0.0f);
    if (ScenePicker::localBounds(*node, localMin, localMax)) {
        const glm::mat4 world = node->getWorldMatrix();
        for (int corner = 0; corner < 8; ++corner) {
            const glm::vec3 localCorner(
                (corner & 1) ? localMax.x : localMin.x,
                (corner & 2) ? localMax.y : localMin.y,
                (corner & 4) ? localMax.z : localMin.z);
            const glm::vec3 worldCorner = glm::vec3(world * glm::vec4(localCorner, 1.0f));

            if (!hasBounds) {
                outMin = worldCorner;
                outMax = worldCorner;
                hasBounds = true;
            } else {
                outMin = glm::min(outMin, worldCorner);
                outMax = glm::max(outMax, worldCorner);
            }
        }
    }

    for (size_t i = 0; i < node->getChildCount(); ++i) {
        accumulateWorldBounds(node->getChild(i), outMin, outMax, hasBounds);
    }
}

bool EditorSystem::focusCameraOnNode(const std::shared_ptr<SceneNode>& node) {
    if (!node || !canFocusCamera()) {
        return false;
    }

    glm::vec3 boundsMin(0.0f);
    glm::vec3 boundsMax(0.0f);
    bool hasBounds = false;
    accumulateWorldBounds(node, boundsMin, boundsMax, hasBounds);
    if (!hasBounds) {
        return false;
    }

    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    const float radius = glm::max(glm::length(boundsMax - boundsMin) * 0.5f, 0.25f);

    auto& transform = editorCamera->getTransform();

    const glm::vec3 forward = glm::normalize(transform.getRotation() * glm::vec3(0.0f, 0.0f, -1.0f));

    float distance = radius * 3.0f;
    if (auto* cameraComponent = editorCamera->getComponent<CameraComponent>()) {
        const float halfFov = glm::radians(cameraComponent->getFOV()) * 0.5f;
        if (halfFov > 0.01f) {
            distance = (radius / std::tan(halfFov)) * 1.3f;
        }
    }

    transform.setPosition(center - forward * distance);
    return true;
}

void EditorSystem::clearSelection() {
    auto selected = selectedNode.lock();
    if (selected) {
        selected->setSelected(false);
    }
    selectedNode.reset();
    
    if (activeScene) {
        activeScene->clearSelection();
    }
}

bool EditorSystem::isAnyWindowHovered() const {
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
}

void EditorSystem::createDefaultScene() {
    activeScene = std::make_shared<Scene>("Default Scene");
    
    editorCamera = std::make_shared<SceneNode>("Editor Camera");
    auto editorCameraComponent = editorCamera->addComponent<CameraComponent>();
    editorCameraComponent->setFOV(45.0f);
    editorCameraComponent->setActive(false);
    
    editorCamera->getTransform().setPosition(glm::vec3(0, 0, 5));
    editorCamera->getTransform().setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    
    auto gameCameraNode = activeScene->createNode("Game Camera");
    auto gameCameraComponent = gameCameraNode->addComponent<CameraComponent>();
    gameCameraComponent->setFOV(45.0f);
    gameCameraComponent->setActive(true);
    gameCameraNode->getTransform().setPosition(glm::vec3(0, 0, 5));
    activeScene->getRootNode()->addChild(gameCameraNode);
    activeScene->setActiveCamera(gameCameraNode);
}

void EditorSystem::setGridSize(int sizeX, int sizeZ) {
    gridSizeX = sizeX > 0 ? sizeX : 1;
    gridSizeZ = sizeZ > 0 ? sizeZ : 1;
    gridMeshDirty = true;
}

glm::vec3 EditorSystem::snapWorldToGrid(const glm::vec3& worldPos, bool snapY) const {
    glm::vec3 p = worldPos - gridOrigin;
    float invCell = 1.0f / gridCellSize;
    float gx = std::floor(p.x * invCell + 0.5f);
    float gz = std::floor(p.z * invCell + 0.5f);
    glm::vec3 snapped(gridOrigin.x + gx * gridCellSize,
                      snapY ? (gridOrigin.y + std::floor(p.y * invCell + 0.5f) * gridCellSize) : worldPos.y,
                      gridOrigin.z + gz * gridCellSize);
    return snapped;
}

void EditorSystem::worldToGridCell(const glm::vec3& worldPos, int& outGx, int& outGz) const {
    glm::vec3 p = worldPos - gridOrigin;
    float invCell = 1.0f / gridCellSize;
    outGx = static_cast<int>(std::floor(p.x * invCell));
    outGz = static_cast<int>(std::floor(p.z * invCell));
}

glm::vec3 EditorSystem::gridCellToWorld(int gx, int gz, float y) const {
    return glm::vec3(gridOrigin.x + (gx + 0.5f) * gridCellSize,
                    y,
                    gridOrigin.z + (gz + 0.5f) * gridCellSize);
}

bool EditorSystem::isGridCellInBounds(int gx, int gz) const {
    return gx >= 0 && gx < gridSizeX && gz >= 0 && gz < gridSizeZ;
}

void EditorSystem::buildGridMesh() {
    if (gridVao) {
        glDeleteVertexArrays(1, &gridVao);
        glDeleteBuffers(1, &gridVbo);
        glDeleteBuffers(1, &gridIbo);
        gridVao = gridVbo = gridIbo = 0;
    }
    int slicesX = gridSizeX;
    int slicesZ = gridSizeZ;
    if (slicesX < 1 || slicesZ < 1) return;
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;
    const float halfX = (float)slicesX * 0.5f * gridCellSize;
    const float halfZ = (float)slicesZ * 0.5f * gridCellSize;
    for (int j = 0; j <= slicesZ; ++j) {
        for (int i = 0; i <= slicesX; ++i) {
            float x = gridOrigin.x - halfX + (float)i * gridCellSize;
            float z = gridOrigin.z - halfZ + (float)j * gridCellSize;
            vertices.push_back(glm::vec3(x, gridOrigin.y, z));
        }
    }
    for (int j = 0; j < slicesZ; ++j) {
        for (int i = 0; i < slicesX; ++i) {
            int row1 = j * (slicesX + 1);
            int row2 = (j + 1) * (slicesX + 1);
            indices.push_back((unsigned int)(row1 + i));
            indices.push_back((unsigned int)(row1 + i + 1));
            indices.push_back((unsigned int)(row1 + i + 1));
            indices.push_back((unsigned int)(row2 + i + 1));
            indices.push_back((unsigned int)(row2 + i + 1));
            indices.push_back((unsigned int)(row2 + i));
            indices.push_back((unsigned int)(row2 + i));
            indices.push_back((unsigned int)(row1 + i));
        }
    }
    gridLineCount = (GLsizei)indices.size();
    if (vertices.empty() || indices.empty()) return;
    glGenVertexArrays(1, &gridVao);
    glBindVertexArray(gridVao);
    glGenBuffers(1, &gridVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(glm::vec3)), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glGenBuffers(1, &gridIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gridMeshDirty = false;
}

void EditorSystem::renderGridInViewport(CameraComponent* camera) {
    if (!camera || !showGrid) return;
    if (gridMeshDirty || gridVao == 0) {
        buildGridMesh();
        if (gridVao == 0) return;
    }
    if (!gridShader) {
        gridShader = std::make_shared<Shader>();
        if (!gridShader->loadFromFiles("assets/linux_shaders/grid.vert", "assets/linux_shaders/grid.frag")) {
            gridShader.reset();
            return;
        }
    }
    if (!gridShader->isValid() || gridLineCount == 0) return;
    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 projection = camera->getProjectionMatrix();
    glm::mat4 mvp = projection * view;
    glEnable(GL_DEPTH_TEST);
    gridShader->use();
    gridShader->setMat4("mvp", mvp);
    gridShader->setVec4("color", glm::vec4(0.35f, 0.35f, 0.35f, 0.8f));
    glBindVertexArray(gridVao);
    glDrawElements(GL_LINES, gridLineCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void EditorSystem::setCameraMode(CameraMode mode) {
    cameraMode = mode;
    
    if (!activeScene) return;
    
    if (editorCamera) {
        auto editorCameraComponent = editorCamera->getComponent<CameraComponent>();
        if (editorCameraComponent) {
            editorCameraComponent->setActive(mode == CameraMode::EDITOR_CAMERA);
        }
    }
    
    auto gameCamera = activeScene->getActiveCamera();
    if (gameCamera) {
        auto gameCameraComponent = gameCamera->getComponent<CameraComponent>();
        if (gameCameraComponent) {
            gameCameraComponent->setActive(mode == CameraMode::GAME_CAMERA);
        }
    }
}

std::shared_ptr<SceneNode> EditorSystem::getActiveCamera() const {
    if (!activeScene) return nullptr;
    
    switch (cameraMode) {
        case CameraMode::EDITOR_CAMERA: {
            if (editorCamera && editorCamera->isActive() && editorCamera->isVisible()) {
                return editorCamera;
            }
            return nullptr;
        }
        case CameraMode::GAME_CAMERA: {
            auto gameCamera = activeScene->getActiveCamera();
            if (gameCamera) {
                auto cameraComponent = gameCamera->getComponent<CameraComponent>();
                if (cameraComponent && cameraComponent->isActive() && gameCamera->isVisible() && gameCamera->isActive()) {
                    return gameCamera;
                }
            }
            return nullptr;
        }
        default:
            return nullptr;
    }
}

void EditorSystem::handleViewportInput() {
    if (cameraMode != CameraMode::EDITOR_CAMERA || !editorCamera) {
        return;
    }

    auto& inputManager = GetEngine().getInputManager();
    auto& time = GetEngine().getTime();
    float deltaTime = time.getDeltaTime();

    float moveSpeed = 10.0f * deltaTime;

    auto& transform = editorCamera->getTransform();
    glm::vec3 position = transform.getPosition();

    glm::quat rotation = updateFlyRotation(inputManager.getMouseDelta());

    if (inputManager.isKeyHeld(GLFW_KEY_W)) {
        position += rotation * glm::vec3(0, 0, -1) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_S)) {
        position += rotation * glm::vec3(0, 0,  1) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_A)) {
        position += rotation * glm::vec3(-1, 0, 0) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_D)) {
        position += rotation * glm::vec3( 1, 0, 0) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_SPACE)) {
        position += glm::vec3(0, 1, 0) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_LEFT_SHIFT)) {
        position += glm::vec3(0, -1, 0) * moveSpeed;
    }

    transform.setPosition(position);
    transform.setRotation(rotation);
}

void EditorSystem::handleGameCameraInput(float deltaTime) {
    if (!activeScene) return;

    auto gameCamera = activeScene->getActiveCamera();
    if (!gameCamera) return;

    auto cameraComponent = gameCamera->getComponent<CameraComponent>();
    if (!cameraComponent) return;

    auto& inputManager = GetEngine().getInputManager();
    float moveSpeed = 10.0f * deltaTime;

    auto& transform = gameCamera->getTransform();
    glm::vec3 position = transform.getPosition();

    glm::quat rotation = updateFlyRotation(inputManager.getMouseDelta());

    if (inputManager.isKeyHeld(GLFW_KEY_W)) {
        position += rotation * glm::vec3(0, 0, -1) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_S)) {
        position += rotation * glm::vec3(0, 0,  1) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_A)) {
        position += rotation * glm::vec3(-1, 0, 0) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_D)) {
        position += rotation * glm::vec3( 1, 0, 0) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_SPACE)) {
        position += glm::vec3(0, 1, 0) * moveSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_LEFT_SHIFT)) {
        position += glm::vec3(0, -1, 0) * moveSpeed;
    }

    transform.setPosition(position);
    transform.setRotation(rotation);
}

std::string EditorSystem::generateUniqueNodeName(const std::string& baseName) {
    if (!activeScene) return baseName;

    if (!activeScene->findNode(baseName)) {
        return baseName;
    }

    size_t numStart = baseName.size();
    while (numStart > 0 && std::isdigit(static_cast<unsigned char>(baseName[numStart - 1]))) {
        --numStart;
    }

    std::string root = baseName.substr(0, numStart);
    int nextNumber = 1;
    if (numStart < baseName.size()) {
        nextNumber = std::stoi(baseName.substr(numStart)) + 1;
    }

    while (!root.empty() && std::isspace(static_cast<unsigned char>(root.back()))) {
        root.pop_back();
    }

    std::string name;
    do {
        name = root + std::to_string(nextNumber);
        ++nextNumber;
    } while (activeScene->findNode(name));

    return name;
}

void EditorSystem::makeSubtreeNamesUnique(std::shared_ptr<SceneNode> node) {
    if (!node) {
        return;
    }

    for (size_t i = 0; i < node->getChildCount(); ++i) {
        auto child = node->getChild(i);
        if (child) {
            child->setName(generateUniqueNodeName(child->getName()));
            makeSubtreeNamesUnique(child);
        }
    }
}

std::shared_ptr<SceneNode> EditorSystem::instantiateNodeSubtree(std::shared_ptr<SceneNode> source,
                                                                std::shared_ptr<SceneNode> parent,
                                                                const std::string& rootNameSuffix) {
    if (!source || !parent || !activeScene) {
        return nullptr;
    }

    auto instance = SceneSerializer::duplicateNodeSubtree(source);
    if (!instance) {
        return nullptr;
    }

    instance->setName(generateUniqueNodeName(source->getName() + rootNameSuffix));
    makeSubtreeNamesUnique(instance);
    parent->addChild(instance);
    instance->start();
    return instance;
}

std::shared_ptr<SceneNode> EditorSystem::instantiateTemplate(const std::string& filepath,
                                                             std::shared_ptr<SceneNode> parent) {
    if (!parent || !activeScene || filepath.empty()) {
        return nullptr;
    }

    auto instance = NodeTemplateSerializer::loadNodeTemplate(filepath);
    if (!instance) {
        return nullptr;
    }

    instance->setName(generateUniqueNodeName(instance->getName()));
    makeSubtreeNamesUnique(instance);
    parent->addChild(instance);
    instance->start();
    return instance;
}

int EditorSystem::getSiblingIndex(std::shared_ptr<SceneNode> node) const {
    if (!node || !node->getParent()) {
        return -1;
    }
    auto parent = node->getParent();
    for (size_t i = 0; i < parent->getChildCount(); ++i) {
        if (parent->getChild(i) == node) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int EditorSystem::getNodeDepth(std::shared_ptr<SceneNode> node) {
    if (!node) {
        return -1;
    }
    int depth = 0;
    SceneNode* current = node->getParent();
    while (current) {
        ++depth;
        current = current->getParent();
    }
    return depth;
}

bool EditorSystem::canMoveNodeUp(std::shared_ptr<SceneNode> node) const {
    return getSiblingIndex(node) > 0;
}

bool EditorSystem::canMoveNodeDown(std::shared_ptr<SceneNode> node) const {
    if (!node || !node->getParent()) {
        return false;
    }
    int index = getSiblingIndex(node);
    return index >= 0 && index < static_cast<int>(node->getParent()->getChildCount()) - 1;
}

void EditorSystem::moveNodeUp(std::shared_ptr<SceneNode> node) {
    if (!canMoveNodeUp(node)) {
        return;
    }
    auto parent = node->getParent();
    int index = getSiblingIndex(node);
    parent->reorderChild(static_cast<size_t>(index), static_cast<size_t>(index - 1));
}

void EditorSystem::moveNodeDown(std::shared_ptr<SceneNode> node) {
    if (!canMoveNodeDown(node)) {
        return;
    }
    auto parent = node->getParent();
    int index = getSiblingIndex(node);
    parent->reorderChild(static_cast<size_t>(index), static_cast<size_t>(index + 1));
}

bool EditorSystem::reorderNodeBefore(std::shared_ptr<SceneNode> dragged, std::shared_ptr<SceneNode> target) {
    if (!dragged || !target || dragged == target || !activeScene) {
        return false;
    }
    if (dragged == activeScene->getRootNode()) {
        return false;
    }

    auto draggedParent = dragged->getParent();
    auto targetParent = target->getParent();
    if (!draggedParent || draggedParent != targetParent) {
        return false;
    }

    int fromIndex = getSiblingIndex(dragged);
    int toIndex = getSiblingIndex(target);
    if (fromIndex < 0 || toIndex < 0 || fromIndex == toIndex) {
        return false;
    }

    size_t insertIndex = static_cast<size_t>(toIndex);
    if (fromIndex < toIndex) {
        insertIndex = static_cast<size_t>(toIndex - 1);
    }
    draggedParent->reorderChild(static_cast<size_t>(fromIndex), insertIndex);
    return true;
}

std::shared_ptr<SceneNode> EditorSystem::findNodeShared(SceneNode* nodePtr) const {
    if (!nodePtr || !activeScene) {
        return nullptr;
    }

    std::function<std::shared_ptr<SceneNode>(std::shared_ptr<SceneNode>)> search =
        [&](std::shared_ptr<SceneNode> current) -> std::shared_ptr<SceneNode> {
            if (!current) {
                return nullptr;
            }
            if (current.get() == nodePtr) {
                return current;
            }
            for (size_t i = 0; i < current->getChildCount(); ++i) {
                if (auto found = search(current->getChild(i))) {
                    return found;
                }
            }
            return nullptr;
        };

    return search(activeScene->getRootNode());
}

void EditorSystem::selectAllChildren(std::shared_ptr<SceneNode> node) {
    if (!node) {
        return;
    }
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        if (auto child = node->getChild(i)) {
            child->setSelected(true);
        }
    }
}

void EditorSystem::deleteNode(std::shared_ptr<SceneNode> node) {
    if (!node || !activeScene) {
        return;
    }
    if (node == activeScene->getRootNode()) {
        return;
    }

    pendingNodeDeletions_.push_back(node);
}

void EditorSystem::processPendingNodeDeletions() {
    if (pendingNodeDeletions_.empty()) {
        return;
    }

    auto pending = std::move(pendingNodeDeletions_);
    pendingNodeDeletions_.clear();

    for (const auto& node : pending) {
        if (!node || !activeScene) {
            continue;
        }
        if (!node->getParent()) {
            continue;
        }
        deleteNodeImmediate(node);
    }
}

void EditorSystem::deleteNodeImmediate(std::shared_ptr<SceneNode> node) {
    if (!node || !activeScene) {
        return;
    }

    if (node == activeScene->getRootNode()) {
        return;
    }

    if (!node->getParent()) {
        return;
    }

    auto selected = selectedNode.lock();
    if (selected && isInSubtree(selected, node)) {
        clearSelection();
    }

    auto sceneSelected = activeScene->getSelectedNode();
    if (sceneSelected && isInSubtree(sceneSelected, node)) {
        activeScene->clearSelection();
    }

    auto activeCamera = activeScene->getActiveCamera();
    const bool cameraInSubtree = activeCamera && isInSubtree(activeCamera, node);

    auto activeSkybox = activeScene->getActiveSkybox();
    const bool skyboxInSubtree = activeSkybox && isInSubtree(activeSkybox, node);

    if (cameraInSubtree) {
        activeScene->setActiveCamera(nullptr);
    }
    if (skyboxInSubtree) {
        activeScene->setActiveSkybox(nullptr);
    }

    destroyComponentsPostOrder(node);
    detachNodeFromScene(node);

    if (cameraInSubtree) {
        if (auto newCamera = findFirstCameraExcluding(activeScene->getRootNode(), node)) {
            activeScene->setActiveCamera(newCamera);
        }
    }
}

bool EditorSystem::isInSubtree(std::shared_ptr<SceneNode> candidate, std::shared_ptr<SceneNode> subtreeRoot) const {
    if (!candidate || !subtreeRoot) {
        return false;
    }
    if (candidate == subtreeRoot) {
        return true;
    }
    SceneNode* parent = candidate->getParent();
    while (parent) {
        if (parent == subtreeRoot.get()) {
            return true;
        }
        parent = parent->getParent();
    }
    return false;
}

void EditorSystem::destroyComponentsPostOrder(std::shared_ptr<SceneNode> node) {
    if (!node) {
        return;
    }
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        destroyComponentsPostOrder(node->getChild(i));
    }
    for (auto& component : node->getAllComponents()) {
        if (component) {
            component->destroy();
        }
    }
}

std::shared_ptr<SceneNode> EditorSystem::findFirstCameraExcluding(std::shared_ptr<SceneNode> current,
                                                                  std::shared_ptr<SceneNode> excludedSubtree) const {
    if (!current || isInSubtree(current, excludedSubtree)) {
        return nullptr;
    }
    if (current->getComponent<CameraComponent>()) {
        return current;
    }
    for (size_t i = 0; i < current->getChildCount(); ++i) {
        if (auto found = findFirstCameraExcluding(current->getChild(i), excludedSubtree)) {
            return found;
        }
    }
    return nullptr;
}

bool EditorSystem::detachNodeFromScene(std::shared_ptr<SceneNode> node) {
    if (!node || !activeScene) {
        return false;
    }
    return detachNodeRecursive(activeScene->getRootNode(), node);
}

bool EditorSystem::detachNodeRecursive(std::shared_ptr<SceneNode> current, std::shared_ptr<SceneNode> target) {
    if (!current || !target) {
        return false;
    }
    for (size_t i = 0; i < current->getChildCount(); ++i) {
        if (current->getChild(i) == target) {
            current->removeChild(target);
            return true;
        }
        if (detachNodeRecursive(current->getChild(i), target)) {
            return true;
        }
    }
    return false;
}

void EditorSystem::renderSceneToViewport() {
    if (!activeScene || !viewportFramebuffer) return;
    
    auto cameraNode = getActiveCamera();
    if (!cameraNode) {
        viewportFramebuffer->bind();
        viewportFramebuffer->clear(glm::vec3(0.1f, 0.1f, 0.1f));
        viewportFramebuffer->unbind();
        return;
    }
    
    auto cameraComponent = cameraNode->getComponent<CameraComponent>();
    if (!cameraComponent) {
        viewportFramebuffer->bind();
        viewportFramebuffer->clear(glm::vec3(0.1f, 0.1f, 0.1f));
        viewportFramebuffer->unbind();
        return;
    }
    
    GLint currentFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFramebuffer);
    
    GLint currentViewport[4];
    glGetIntegerv(GL_VIEWPORT, currentViewport);
    
    viewportFramebuffer->bind();
    viewportFramebuffer->clear(glm::vec3(0.2f, 0.3f, 0.3f));

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    
    auto& lightingManager = LightingManager::getInstance();
    lightingManager.update();

    lightingManager.beginPass();

    renderSceneDirectly(*activeScene, cameraComponent);
    if (showGrid) {
        renderGridInViewport(cameraComponent);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, currentFramebuffer);
    glViewport(currentViewport[0], currentViewport[1], currentViewport[2], currentViewport[3]);
}

void EditorSystem::renderSceneDirectly(Scene& scene, CameraComponent* camera) {
    if (!camera) return;

    syncNavGridFromScene();

    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 projectionMatrix = camera->getProjectionMatrix();
    
    bool isEditorCamera = (cameraMode == CameraMode::EDITOR_CAMERA);
    
    auto rootNode = scene.getRootNode();
    renderShadowPassDirectly(camera);
    renderSkyboxDirectly(scene, camera, viewMatrix, projectionMatrix);
    if (rootNode) {
        // opaque, cutout, then blended (see Material.h)
        for (int renderPass = 1; renderPass <= 3; ++renderPass) {
            renderNodeDirectly(rootNode, glm::mat4(1.0f), viewMatrix, projectionMatrix, isEditorCamera, renderPass);
        }
    }
    
    renderPhysicsDebugShapes(viewMatrix, projectionMatrix);
    if (showNavMeshDebug) {
        renderNavMeshDebug(viewMatrix, projectionMatrix);
    }
    if (rootNode) {
        renderScreenSpaceTextDirectly(rootNode, isEditorCamera);
    }
}

void EditorSystem::collectShadowCasters(std::shared_ptr<SceneNode> node, const glm::mat4& parentTransform) {
    if (!node || !node->isVisible() || !node->isActive()) return;

    glm::mat4 worldTransform = parentTransform * node->getLocalMatrix();

    auto meshRenderer = node->getComponent<MeshRenderer>();
    if (meshRenderer && meshRenderer->isEnabled() && meshRenderer->getCastShadows()) {
        auto mesh = meshRenderer->getMesh();
        if (mesh) {
            shadowCasters.push_back({mesh, worldTransform});
        }
    }

    auto modelRenderer = node->getComponent<ModelRenderer>();
    if (modelRenderer && modelRenderer->isEnabled() && modelRenderer->getCastShadows()
        && modelRenderer->isModelLoaded()) {
        auto meshes = modelRenderer->getMeshes();
        const auto& meshNodeTransforms = modelRenderer->getMeshNodeTransforms();
        for (size_t i = 0; i < meshes.size(); ++i) {
            if (!meshes[i]) continue;
            glm::mat4 gltfNodeTransform = (i < meshNodeTransforms.size())
                ? meshNodeTransforms[i]
                : glm::mat4(1.0f);
            shadowCasters.push_back({meshes[i], worldTransform * gltfNodeTransform});
        }
    }

    for (size_t i = 0; i < node->getChildCount(); ++i) {
        auto child = node->getChild(i);
        if (child) {
            collectShadowCasters(child, worldTransform);
        }
    }
}

void EditorSystem::renderShadowPassDirectly(CameraComponent* camera) {
    shadowAtlasReady = false;
    LightingManager::getInstance().setShadowMapBound(false);

    if (!camera || !activeScene) return;

    auto& shadowManager = ShadowManager::getInstance();
    const glm::vec3 cameraPosition = glm::vec3(glm::inverse(camera->getViewMatrix())[3]);
    shadowManager.update(cameraPosition, camera->getForward());

    const std::vector<ShadowView>& views = shadowManager.getViews();
    if (views.empty()) {
        return;
    }

    shadowCasters.clear();
    collectShadowCasters(activeScene->getRootNode(), glm::mat4(1.0f));
    if (shadowCasters.empty()) {
        return;
    }

    auto shadowShader = Shader::getShadowDepthShader();
    if (!shadowShader || !shadowShader->isValid()) {
        return;
    }

    if (!shadowAtlas.ensureCreated(shadowManager.getAtlasWidth(), shadowManager.getAtlasHeight())) {
        return;
    }

    // The scene is being drawn into the viewport framebuffer, so restore that
    // rather than assuming the default one
    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    shadowAtlas.bindForWriting();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);

    shadowShader->use();
    // The editor draws models in bind pose, so the casters must not be skinned
    // either or the shadow would not line up with the mesh on screen
    shadowShader->setInt("u_NumBones", 0);

    for (const ShadowView& view : views) {
        const glm::ivec4 tile = shadowManager.getTileViewport(view.tile);
        glViewport(tile.x, tile.y, tile.z, tile.w);

        shadowShader->setMat4("u_LightViewProj", view.viewProjection);

        for (const ShadowCaster& caster : shadowCasters) {
            const glm::vec3 boundsMin = caster.mesh->getBoundsMin();
            const glm::vec3 boundsMax = caster.mesh->getBoundsMax();
            if (Frustum::areBoundsValid(boundsMin, boundsMax)
                && !view.frustum.containsAABB(boundsMin, boundsMax, caster.modelMatrix)) {
                continue;
            }

            shadowShader->setMat4("modelMatrix", caster.modelMatrix);
            caster.mesh->draw();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    shadowAtlas.bindTexture(kShadowMapTextureUnit);
    glActiveTexture(GL_TEXTURE0);

    shadowAtlasReady = true;
    LightingManager::getInstance().setShadowMapBound(true);
}

void EditorSystem::renderSkyboxDirectly(Scene& scene, CameraComponent* camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!camera) return;
    
    auto activeSkyboxNode = scene.getActiveSkybox();
    if (!activeSkyboxNode) return;
    
    auto skyboxComp = activeSkyboxNode->getComponent<SkyboxComponent>();
    if (!skyboxComp || !skyboxComp->isActive()) return;
    
    auto cubemapTexture = skyboxComp->getCubemapTexture();
    auto skyboxMesh = skyboxComp->getSkyboxMesh();
    auto skyboxMaterial = skyboxComp->getSkyboxMaterial();
    
    if (!cubemapTexture || !skyboxMesh || !skyboxMaterial) return;
    
    GLboolean cullFaceEnabled;
    glGetBooleanv(GL_CULL_FACE, &cullFaceEnabled);
    if (cullFaceEnabled) {
        glDisable(GL_CULL_FACE);
    }
    
    glm::mat4 skyboxViewMatrix = glm::mat4(glm::mat3(viewMatrix));
    
    skyboxMaterial->apply();
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    
    auto shader = skyboxMaterial->getShader();
    if (shader && shader->isValid()) {
        shader->use();
        shader->setMat4("view", skyboxViewMatrix);
        shader->setMat4("projection", projectionMatrix);
        shader->setInt("skybox", 0);
        
        cubemapTexture->bindCubemap(0);
    }
    
    skyboxMesh->bind();
    skyboxMesh->draw();
    skyboxMesh->unbind();
    
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    
    if (cullFaceEnabled) {
        glEnable(GL_CULL_FACE);
    }
}

void EditorSystem::renderNodeDirectly(std::shared_ptr<SceneNode> node, const glm::mat4& parentTransform,
                                    const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, bool isEditorCamera,
                                    int renderPass) {
    if (!node || !node->isVisible() || !node->isActive()) return;
    
    glm::mat4 worldTransform = parentTransform * node->getLocalMatrix();
    
    auto meshRenderer = node->getComponent<MeshRenderer>();
    if (meshRenderer && meshRenderer->isEnabled()) {
        auto mesh = meshRenderer->getMesh();
        auto material = meshRenderer->getRenderMaterial();

        if (mesh && material) {
            const int materialPass = static_cast<int>(getRenderSortBucket(*material)) + 1;
            if (renderPass != materialPass) { /* drawn in its own pass */ }
            else {
                if (activeScene) {
                    auto activeSkyboxNode = activeScene->getActiveSkybox();
                    if (activeSkyboxNode) {
                        auto skyboxComp = activeSkyboxNode->getComponent<SkyboxComponent>();
                        if (skyboxComp && skyboxComp->isActive()) {
                            auto envMap = skyboxComp->getCubemapTexture();
                            if (envMap) {
                                material->setTexture("u_EnvironmentMap", envMap);
                                material->setBool("u_HasEnvironmentMap", true);
                            } else {
                                material->setBool("u_HasEnvironmentMap", false);
                            }
                        } else {
                            material->setBool("u_HasEnvironmentMap", false);
                        }
                    } else {
                        material->setBool("u_HasEnvironmentMap", false);
                    }
                }

                material->apply();
                
                auto shader = material->getShader();
                if (shader) {
                    shader->setMat4("modelMatrix", worldTransform);
                    shader->setMat4("viewMatrix", viewMatrix);
                    shader->setMat4("projectionMatrix", projectionMatrix);
                    
                    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));
                    shader->setMat3("normalMatrix", normalMatrix);
                    
                    glm::vec3 cameraPos = glm::vec3(glm::inverse(viewMatrix)[3]);
                    shader->setVec3("u_CameraPos", cameraPos);

                    shader->setInt("u_ReceiveShadows",
                        (shadowAtlasReady && meshRenderer->getReceiveShadows()) ? 1 : 0);

                    auto& lightingManager = LightingManager::getInstance();
                    size_t numLights = lightingManager.getActiveLightCount();

                    if (numLights > 0) {
                        try {
                            auto lightDataArray = lightingManager.getLightDataArray();
                            shader->setInt("u_NumLights", (int)numLights);

                            for (size_t i = 0; i < numLights; ++i) {
                                const auto& lightData = lightDataArray[i];
                                std::string lightName = "u_Lights[" + std::to_string(i) + "]";
                                
                                shader->setVec4(lightName + ".position", lightData.position);
                                shader->setVec4(lightName + ".direction", lightData.direction);
                                shader->setVec4(lightName + ".color", lightData.color);
                                shader->setVec4(lightName + ".params", lightData.params);
                                shader->setVec4(lightName + ".attenuation", lightData.attenuation);
                            }
                        } catch (...) {
                            shader->setInt("u_NumLights", 0);
                        }
                    } else {
                        shader->setInt("u_NumLights", 0);
                    }
                }
                
                mesh->draw();
                if (!material->getDepthWrite()) {
                    glDepthMask(GL_TRUE);
                    glDepthFunc(GL_LESS);
                }
            }
        }
    }
    
    auto modelRenderer = node->getComponent<ModelRenderer>();
    if (modelRenderer && modelRenderer->isEnabled()) {
        if (modelRenderer->isModelLoaded()) {
            auto meshes = modelRenderer->getMeshes();
            const auto& meshNodeTransforms = modelRenderer->getMeshNodeTransforms();

            for (size_t i = 0; i < meshes.size(); ++i) {
                auto mesh = meshes[i];
                if (!mesh) continue;

                std::shared_ptr<Material> material = modelRenderer->getRenderMaterial(i);
                if (!material) {
                    material = Material::getDefaultMaterial();
                }
                if (!material) continue;

                if (renderPass != static_cast<int>(getRenderSortBucket(*material)) + 1) continue;

                glm::mat4 gltfNodeTransform = (i < meshNodeTransforms.size())
                    ? meshNodeTransforms[i]
                    : glm::mat4(1.0f);
                glm::mat4 modelMatrix = worldTransform * gltfNodeTransform;

                GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
                if (material->getDoubleSided() && cullWasEnabled) {
                    glDisable(GL_CULL_FACE);
                }

                if (activeScene) {
                    auto activeSkyboxNode = activeScene->getActiveSkybox();
                    if (activeSkyboxNode) {
                        auto skyboxComp = activeSkyboxNode->getComponent<SkyboxComponent>();
                        if (skyboxComp && skyboxComp->isActive()) {
                            auto envMap = skyboxComp->getCubemapTexture();
                            if (envMap) {
                                material->setTexture("u_EnvironmentMap", envMap);
                                material->setBool("u_HasEnvironmentMap", true);
                            } else {
                                material->setBool("u_HasEnvironmentMap", false);
                            }
                        } else {
                            material->setBool("u_HasEnvironmentMap", false);
                        }
                    } else {
                        material->setBool("u_HasEnvironmentMap", false);
                    }
                }
                
                material->apply();
                
                auto shader = material->getShader();
                if (shader) {
                    shader->setMat4("modelMatrix", modelMatrix);
                    shader->setMat4("viewMatrix", viewMatrix);
                    shader->setMat4("projectionMatrix", projectionMatrix);
                    
                    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
                    shader->setMat3("normalMatrix", normalMatrix);
                    
                    glm::vec3 cameraPos = glm::vec3(glm::inverse(viewMatrix)[3]);
                    shader->setVec3("u_CameraPos", cameraPos);

                    shader->setInt("u_ReceiveShadows",
                        (shadowAtlasReady && modelRenderer->getReceiveShadows()) ? 1 : 0);

                    auto& lightingManager = LightingManager::getInstance();
                    size_t numLights = lightingManager.getActiveLightCount();

                    if (numLights > 0) {
                        try {
                            auto lightDataArray = lightingManager.getLightDataArray();
                            shader->setInt("u_NumLights", (int)numLights);

                            for (size_t j = 0; j < numLights; ++j) {
                                const auto& lightData = lightDataArray[j];
                                std::string lightName = "u_Lights[" + std::to_string(j) + "]";
                                
                                shader->setVec4(lightName + ".position", lightData.position);
                                shader->setVec4(lightName + ".direction", lightData.direction);
                                shader->setVec4(lightName + ".color", lightData.color);
                                shader->setVec4(lightName + ".params", lightData.params);
                                shader->setVec4(lightName + ".attenuation", lightData.attenuation);
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "Error setting light uniforms: " << e.what() << std::endl;
                        }
                    } else {
                        shader->setInt("u_NumLights", 0);
                    }
                }
                
                mesh->draw();
                if (!material->getDepthWrite()) {
                    glDepthMask(GL_TRUE);
                    glDepthFunc(GL_LESS);
                }
                if (material->getDoubleSided() && cullWasEnabled) {
                    glEnable(GL_CULL_FACE);
                }
            }
        }
    }
    
#ifdef EDITOR_BUILD
    if (renderPass == 1) {
    auto area3DComponent = node->getComponent<Area3DComponent>();
    if (area3DComponent && area3DComponent->getShowDebugShape()) {
        area3DComponent->renderDebugWireframe(viewMatrix, projectionMatrix);
    }
#endif
    
    auto textComponent = node->getComponent<TextComponent>();
    if (textComponent && textComponent->isEnabled()) {
        if (textComponent->getRenderMode() == TextRenderMode::WORLD_SPACE) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_DEPTH_TEST);

            textComponent->renderWorldSpaceDirectly(worldTransform, viewMatrix, projectionMatrix);

            glDisable(GL_BLEND);
        }
    }
    
#ifdef EDITOR_BUILD
    auto lightComponent = node->getComponent<LightComponent>();
    if (lightComponent && lightComponent->isEnabled() && lightComponent->getShowGizmo()) {
        auto gizmoMesh = lightComponent->getGizmoMesh();
        auto gizmoMaterial = lightComponent->getGizmoMaterial();
        
        if (gizmoMesh && gizmoMaterial) {
            gizmoMaterial->apply();
            
            auto shader = gizmoMaterial->getShader();
            if (shader) {
                shader->setMat4("modelMatrix", worldTransform);
                shader->setMat4("viewMatrix", viewMatrix);
                shader->setMat4("projectionMatrix", projectionMatrix);
                
                glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));
                shader->setMat3("normalMatrix", normalMatrix);
            }
            
            gizmoMesh->draw();
        }
    }
    
    auto cameraComponent = node->getComponent<CameraComponent>();
    if (cameraComponent && cameraComponent->isEnabled()) {
        if (cameraComponent->getShowGizmo()) {
            auto gizmoMesh = cameraComponent->getGizmoMesh();
            auto gizmoMaterial = cameraComponent->getGizmoMaterial();
            
            if (gizmoMesh && gizmoMaterial) {
                gizmoMaterial->apply();
                
                auto shader = gizmoMaterial->getShader();
                if (shader) {
                    shader->setMat4("modelMatrix", worldTransform);
                    shader->setMat4("viewMatrix", viewMatrix);
                    shader->setMat4("projectionMatrix", projectionMatrix);
                    
                    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));
                    shader->setMat3("normalMatrix", normalMatrix);
                }
                
                gizmoMesh->draw();
            }
        }
        
        if (cameraComponent->getShowFrustum()) {
            auto frustumMesh = cameraComponent->getFrustumMesh();
            auto frustumMaterial = cameraComponent->getFrustumMaterial();
            
            if (frustumMesh && frustumMaterial) {
                GLint currentPolygonMode[2];
                glGetIntegerv(GL_POLYGON_MODE, currentPolygonMode);
                GLboolean depthTestEnabled;
                glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
                
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glEnable(GL_DEPTH_TEST);
                
                frustumMaterial->apply();
                
                auto shader = frustumMaterial->getShader();
                if (shader) {
                    shader->setMat4("modelMatrix", worldTransform);
                    shader->setMat4("viewMatrix", viewMatrix);
                    shader->setMat4("projectionMatrix", projectionMatrix);
                    shader->setVec3("u_Color", frustumMaterial->getColor());
                }
                
                frustumMesh->draw();
                
                // Restore OpenGL state
                glPolygonMode(GL_FRONT_AND_BACK, currentPolygonMode[0]);
                if (!depthTestEnabled) {
                    glDisable(GL_DEPTH_TEST);
                }
            }
        }
    }
    }
#endif
    
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        auto child = node->getChild(i);
        if (child) {
            renderNodeDirectly(child, worldTransform, viewMatrix, projectionMatrix, isEditorCamera, renderPass);
        }
    }
}

void EditorSystem::renderPhysicsDebugShapes(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
#ifdef EDITOR_BUILD
    if (!activeScene) return;
    
    auto debugMaterial = std::make_shared<Material>();
    debugMaterial->setColor(glm::vec3(0.0f, 1.0f, 0.0f));
    
    auto debugShader = std::make_shared<Shader>();
    std::string vertexShaderSource = 
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 modelMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat4 projectionMatrix;\n"
        "void main() {\n"
        "    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0);\n"
        "}\n";
    
    std::string fragmentShaderSource = 
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec3 u_Color;\n"
        "void main() {\n"
        "    FragColor = vec4(u_Color, 1.0);\n"
        "}\n";
    
    if (debugShader->loadFromSource(vertexShaderSource, fragmentShaderSource)) {
        debugMaterial->setShader(debugShader);
        debugMaterial->setColor(glm::vec3(0.0f, 1.0f, 0.0f));
    }
    
    auto cameraNode = activeScene->getActiveCamera();
    if (!cameraNode) return;
    
    auto cameraComponent = cameraNode->getComponent<CameraComponent>();
    if (!cameraComponent) return;
    
    GLint currentPolygonMode[2];
    glGetIntegerv(GL_POLYGON_MODE, currentPolygonMode);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glDisable(GL_DEPTH_TEST);
    
    std::function<void(std::shared_ptr<SceneNode>)> renderPhysicsComponents = 
        [&](std::shared_ptr<SceneNode> node) {
            if (!node || !node->isVisible() || !node->isActive()) return;
            
            auto physicsComponent = node->getComponent<PhysicsComponent>();
            if (physicsComponent && physicsComponent->isEnabled() &&
                physicsComponent->getShowCollisionShape()) {
                physicsComponent->renderDebugShape(*debugMaterial, viewMatrix, projectionMatrix);
            }
            
            auto raycastComponent = node->getComponent<RaycastComponent>();
            if (raycastComponent && raycastComponent->isEnabled() && raycastComponent->getShowDebugLine()) {
                raycastComponent->renderDebugLine(*debugMaterial, viewMatrix, projectionMatrix);
            }

            for (size_t i = 0; i < node->getChildCount(); ++i) {
                auto child = node->getChild(i);
                if (child) {
                    renderPhysicsComponents(child);
                }
            }
        };
    
    // Start from root node
    auto rootNode = activeScene->getRootNode();
    if (rootNode) {
        renderPhysicsComponents(rootNode);
    }
    
    glPolygonMode(GL_FRONT_AND_BACK, currentPolygonMode[0]);
    if (depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
#endif
}

void EditorSystem::renderScreenSpaceTextDirectly(std::shared_ptr<SceneNode> node, bool isEditorCamera) {
#ifndef EDITOR_BUILD
    (void)node;
    (void)isEditorCamera;
    return;
#else
    if (!node || !node->isVisible() || !node->isActive()) {
        return;
    }

    auto textComponent = node->getComponent<TextComponent>();
    if (textComponent && textComponent->isEnabled() &&
        textComponent->getRenderMode() == TextRenderMode::SCREEN_SPACE &&
        !isEditorCamera) {
        textComponent->renderScreenSpaceDirectly();
    }

    for (size_t i = 0; i < node->getChildCount(); ++i) {
        auto child = node->getChild(i);
        if (child) {
            renderScreenSpaceTextDirectly(child, isEditorCamera);
        }
    }
#endif
}

void EditorSystem::syncNavGridFromScene() {
#ifdef EDITOR_BUILD
    if (!activeScene) return;
    auto root = activeScene->getRootNode();
    if (!root) return;
    std::function<void(std::shared_ptr<SceneNode>)> visit = [&](std::shared_ptr<SceneNode> node) {
        if (!node || !node->isActive()) return;
        auto comp = node->getComponent<NavVolumeComponent>();
        if (comp)
            comp->syncToNavGrid();
        for (size_t i = 0; i < node->getChildCount(); ++i)
            visit(node->getChild(i));
    };
    visit(root);
#endif
}

void EditorSystem::renderNavMeshDebug(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
#ifdef EDITOR_BUILD
    auto debugMaterial = std::make_shared<Material>();
    auto debugShader = std::make_shared<Shader>();
    std::string vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 modelMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat4 projectionMatrix;\n"
        "void main() {\n"
        "    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0);\n"
        "}\n";
    std::string fragmentShaderSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec3 u_Color;\n"
        "void main() {\n"
        "    FragColor = vec4(u_Color, 1.0);\n"
        "}\n";
    if (!debugShader->loadFromSource(vertexShaderSource, fragmentShaderSource))
        return;
    debugMaterial->setShader(debugShader);

    std::shared_ptr<Mesh> cellMesh = Mesh::createWireframePlane(1.0f, 1.0f);
    if (!cellMesh) return;

    GLint currentPolygonMode[2];
    glGetIntegerv(GL_POLYGON_MODE, currentPolygonMode);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glEnable(GL_DEPTH_TEST);

    int gridIndex = 0;
    NavGridRegistry::get().forEachGrid([&](NavGrid* grid) {
        grid->syncObstaclesFromNodes();
        int sizeX = grid->getGridSizeX();
        int sizeZ = grid->getGridSizeZ();
        float cellSize = grid->getCellSize();
        float hue = 0.33f + (gridIndex % 4) * 0.15f;
        if (hue > 1.0f) hue -= 1.0f;
        glm::vec3 baseColor(0.2f, 0.8f, 0.3f);
        for (int iz = 0; iz < sizeZ; ++iz) {
            for (int ix = 0; ix < sizeX; ++ix) {
                glm::vec3 center = grid->cellToWorld(ix, iz);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), center)
                    * glm::scale(glm::mat4(1.0f), glm::vec3(cellSize, 1.0f, cellSize));
                bool obstacle = grid->isTileObstacle(ix, iz);
                glm::vec3 color = obstacle ? glm::vec3(0.9f, 0.2f, 0.2f) : baseColor;
                debugMaterial->setColor(color);
                debugMaterial->apply();
                auto shader = debugMaterial->getShader();
                if (shader) {
                    shader->setMat4("modelMatrix", model);
                    shader->setMat4("viewMatrix", viewMatrix);
                    shader->setMat4("projectionMatrix", projectionMatrix);
                    shader->setVec3("u_Color", debugMaterial->getColor());
                }
                cellMesh->bind();
                cellMesh->draw();
                cellMesh->unbind();
            }
        }
        ++gridIndex;
    });

    glPolygonMode(GL_FRONT_AND_BACK, currentPolygonMode[0]);
    if (!depthTestEnabled)
        glDisable(GL_DEPTH_TEST);
#endif
}

void EditorSystem::setViewportSize(const glm::vec2& size) {
    viewportSize = size;
    
    if (activeScene && size.x > 0 && size.y > 0) {
        auto cameraNode = activeScene->getActiveCamera();
        if (cameraNode) {
            auto cameraComponent = cameraNode->getComponent<CameraComponent>();
            if (cameraComponent) {
                float aspectRatio = size.x / size.y;
                cameraComponent->setAspectRatio(aspectRatio);
            }
        }
    }
}

void EditorSystem::compileSceneBinary(const std::string& jsonPath) {
    const std::string binaryPath = SceneBinaryFormat::jsonPathToBinaryPath(jsonPath);
    if (!SceneBinaryFormat::writeBinarySceneFromJsonFile(jsonPath, binaryPath)) {
        std::cerr << "Failed to compile scene binary: " << binaryPath << std::endl;
    }
}

bool EditorSystem::saveSceneToFile(const std::string& filepath) {
    if (!activeScene) {
        std::cerr << "No active scene to save" << std::endl;
        return false;
    }

    if (!SceneSerializer::saveSceneToFile(activeScene, filepath)) {
        return false;
    }

    activeSceneFilePath_ = AssetPaths::toPortable(filepath);
    compileSceneBinary(activeSceneFilePath_);
    Project::getInstance().addRecentScene(activeSceneFilePath_);
    updateWindowTitle();
    return true;
}

bool EditorSystem::saveActiveScene() {
    if (!activeScene || activeSceneFilePath_.empty()) {
        return false;
    }
    if (!SceneSerializer::saveSceneToFile(activeScene, activeSceneFilePath_)) {
        return false;
    }
    compileSceneBinary(activeSceneFilePath_);
    return true;
}

bool EditorSystem::loadSceneFromFile(const std::string& filepath) {
    LightingManager::getInstance().clearLights();
    // Drop cached ModelRenderer GPU/CPU meshes so materials (blend/cutoff/
    // textures) are rebuilt from the current glTF loader path.
    ModelRenderer::clearMeshCache();

    auto loadedScene = SceneSerializer::loadSceneFromFile(filepath);
    if (loadedScene) {
        setActiveScene(loadedScene);
        activeSceneFilePath_ = AssetPaths::toPortable(filepath);
        Project::getInstance().addRecentScene(activeSceneFilePath_);
        updateWindowTitle();
        return true;
    }
    return false;
}

void EditorSystem::createNewScene() {
    createDefaultScene();
    activeSceneFilePath_.clear();
    clearSelection();
    updateWindowTitle();
}

void EditorSystem::updateWindowTitle() {
    const Project& project = Project::getInstance();
    const std::string scene = activeSceneFilePath_.empty() ? "Untitled Scene" : activeSceneFilePath_;

    GetEngine().setWindowTitle(project.name + " - " + scene + " - Baltrogue Editor");
}

void EditorSystem::requestProjectRestart(const std::string& projectFilePath) {
    requestedProjectRestart_ = projectFilePath;
    GetEngine().setRunning(false);
}

} // namespace GameEngine

#endif // LINUX_BUILD