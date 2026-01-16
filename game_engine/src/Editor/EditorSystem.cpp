#ifdef LINUX_BUILD

#include "Editor/EditorSystem.h"
#include "Editor/EditorUI.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/LightComponent.h"
#include "Components/TextComponent.h"
#include "Components/Area3DComponent.h"
#include "Components/PhysicsComponent.h"
#include "Core/Engine.h"
#include "Rendering/LightingManager.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/Renderer.h"
#include "Physics/PhysicsManager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <iostream>
#include <memory>
#include <algorithm>

namespace GameEngine {

EditorSystem::EditorSystem()
    : activeScene(nullptr)
    , cameraMode(CameraMode::EDITOR_CAMERA)
    , editorCamera(nullptr)
    , viewportFocused(false)
{
    ui = std::unique_ptr<EditorUI>(new EditorUI(*this));
}

EditorSystem::~EditorSystem() {
    shutdown();
}

bool EditorSystem::initialize() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    GLFWwindow* window = glfwGetCurrentContext();
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
        return false;
    }
    
    createDefaultScene();
    
    return true;
}

void EditorSystem::shutdown() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void EditorSystem::update(float deltaTime) {
    bool textInputActive = ImGui::GetIO().WantTextInput;
    
    if (cameraMode == CameraMode::EDITOR_CAMERA && editorCamera && viewportFocused && !textInputActive) {
        handleViewportInput();
    } else if (cameraMode == CameraMode::GAME_CAMERA && viewportFocused && !textInputActive) {
        handleGameCameraInput(deltaTime);
    }
}

void EditorSystem::render() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ui->setupDockspace();
    ui->renderMenuBar();
    ui->renderSceneGraph();
    ui->renderProperties();
    ui->renderViewport();
    ui->renderFileExplorer();
    ui->renderInputMapping();
    
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
    float rotateSpeed = 2.0f * deltaTime;
    
    auto& transform = editorCamera->getTransform();
    glm::vec3 position = transform.getPosition();
    
    static float yaw   = 0.0f;
    static float pitch = 0.0f;

    glm::quat rotation = transform.getRotation();

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

    if (inputManager.isKeyHeld(GLFW_KEY_LEFT)) {
        yaw += rotateSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_RIGHT)) {
        yaw -= rotateSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_UP)) {
        pitch += rotateSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_DOWN)) {
        pitch -= rotateSpeed;
    }

    float limit = glm::half_pi<float>() - 0.01f;
    pitch = glm::clamp(pitch, -limit, limit);

    glm::quat qYaw   = glm::angleAxis(yaw,   glm::vec3(0, 1, 0));
    glm::quat qPitch = glm::angleAxis(pitch, glm::vec3(1, 0, 0));
    rotation = qYaw * qPitch;

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
    float rotateSpeed = 2.0f * deltaTime;

    auto& transform = gameCamera->getTransform();
    glm::vec3 position = transform.getPosition();
    
    static float yaw   = 0.0f;
    static float pitch = 0.0f;

    glm::quat rotation = transform.getRotation();

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
    
    if (inputManager.isKeyHeld(GLFW_KEY_LEFT)) {
        yaw += rotateSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_RIGHT)) {
        yaw -= rotateSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_UP)) {
        pitch += rotateSpeed;
    }
    if (inputManager.isKeyHeld(GLFW_KEY_DOWN)) {
        pitch -= rotateSpeed;
    }

    float limit = glm::half_pi<float>() - 0.01f;
    pitch = glm::clamp(pitch, -limit, limit);

    glm::quat qYaw   = glm::angleAxis(yaw,   glm::vec3(0, 1, 0));
    glm::quat qPitch = glm::angleAxis(pitch, glm::vec3(1, 0, 0));
    rotation = qYaw * qPitch;

    transform.setPosition(position);
    transform.setRotation(rotation);
}

std::string EditorSystem::generateUniqueNodeName(const std::string& baseName) {
    if (!activeScene) return baseName;
    
    std::string name = baseName;
    int counter = 1;
    
    while (activeScene->findNode(name)) {
        name = baseName + " (" + std::to_string(counter) + ")";
        counter++;
    }
    
    return name;
}

void EditorSystem::deleteNode(std::shared_ptr<SceneNode> node) {
    if (!node || !activeScene) return;
    
    if (node == activeScene->getRootNode()) {
        return;
    }
    
    auto selected = selectedNode.lock();
    if (selected == node) {
        clearSelection();
    }
    
    auto activeCamera = activeScene->getActiveCamera();
    bool isDeletingActiveCamera = (activeCamera == node);
    
    if (isDeletingActiveCamera) {
        activeScene->setActiveCamera(nullptr);
    }
    
    auto& components = node->getAllComponents();
    for (auto& component : components) {
        if (component && component->isEnabled()) {
            component->destroy();
        }
    }
    
    std::function<bool(std::shared_ptr<SceneNode>)> findAndRemoveNode = 
        [&](std::shared_ptr<SceneNode> currentNode) -> bool {
            if (!currentNode) return false;
            
            for (size_t i = 0; i < currentNode->getChildCount(); ++i) {
                if (currentNode->getChild(i) == node) {
                    currentNode->removeChild(node);
                    return true;
                }
                
                if (findAndRemoveNode(currentNode->getChild(i))) {
                    return true;
                }
            }
            return false;
        };
    
    findAndRemoveNode(activeScene->getRootNode());
    
    if (isDeletingActiveCamera) {
        std::function<std::shared_ptr<SceneNode>(std::shared_ptr<SceneNode>)> findCamera = 
            [&](std::shared_ptr<SceneNode> currentNode) -> std::shared_ptr<SceneNode> {
                if (!currentNode) return nullptr;
                
                if (currentNode == node) return nullptr;
                
                if (currentNode->getComponent<CameraComponent>()) {
                    return currentNode;
                }
                
                for (size_t i = 0; i < currentNode->getChildCount(); ++i) {
                    auto found = findCamera(currentNode->getChild(i));
                    if (found) return found;
                }
                return nullptr;
            };
        
        auto newCamera = findCamera(activeScene->getRootNode());
        if (newCamera) {
            activeScene->setActiveCamera(newCamera);
        }
    }
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
    
    auto& lightingManager = LightingManager::getInstance();
    lightingManager.update();
    
    renderSceneDirectly(*activeScene, cameraComponent);
    
    glBindFramebuffer(GL_FRAMEBUFFER, currentFramebuffer);
    glViewport(currentViewport[0], currentViewport[1], currentViewport[2], currentViewport[3]);
}

void EditorSystem::renderSceneDirectly(Scene& scene, CameraComponent* camera) {
    if (!camera) return;
    
    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 projectionMatrix = camera->getProjectionMatrix();
    
    bool isEditorCamera = (cameraMode == CameraMode::EDITOR_CAMERA);
    
    auto rootNode = scene.getRootNode();
    if (rootNode) {
        renderNodeDirectly(rootNode, glm::mat4(1.0f), viewMatrix, projectionMatrix, isEditorCamera);
    }
    
    renderPhysicsDebugShapes(viewMatrix, projectionMatrix);
}

void EditorSystem::renderNodeDirectly(std::shared_ptr<SceneNode> node, const glm::mat4& parentTransform,
                                    const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, bool isEditorCamera) {
    if (!node || !node->isVisible() || !node->isActive()) return;
    
    glm::mat4 worldTransform = parentTransform * node->getLocalMatrix();
    
    auto meshRenderer = node->getComponent<MeshRenderer>();
    if (meshRenderer && meshRenderer->isEnabled()) {
        auto mesh = meshRenderer->getMesh();
        auto material = meshRenderer->getMaterial();
        
        if (mesh && material) {
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
        }
    }
    
    auto modelRenderer = node->getComponent<ModelRenderer>();
    if (modelRenderer && modelRenderer->isEnabled()) {
        if (modelRenderer->isModelLoaded()) {
            auto meshes = modelRenderer->getMeshes();
            auto materials = modelRenderer->getMaterials();
            
            for (size_t i = 0; i < meshes.size(); ++i) {
                auto mesh = meshes[i];
                auto material = (i < materials.size()) ? materials[i] : nullptr;
                
                if (!mesh) continue;
                
                if (material) {
                    material->apply();
                    
                    auto shader = material->getShader();
                    if (shader) {
                        shader->setMat4("modelMatrix", worldTransform);
                        shader->setMat4("viewMatrix", viewMatrix);
                        shader->setMat4("projectionMatrix", projectionMatrix);
                        
                        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));
                        shader->setMat3("normalMatrix", normalMatrix);
                        
                        glm::vec3 cameraPos = glm::vec3(glm::inverse(viewMatrix)[3]);
                        shader->setVec3("u_CameraPosition", cameraPos);
                        
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
                }
                
                mesh->draw();
            }
        }
    }
    
#ifdef EDITOR_BUILD
    auto area3DComponent = node->getComponent<Area3DComponent>();
    if (area3DComponent && area3DComponent->getShowDebugShape()) {
        area3DComponent->renderDebugWireframe(viewMatrix, projectionMatrix);
    }
#endif
    
    auto textComponent = node->getComponent<TextComponent>();
    if (textComponent && textComponent->isEnabled()) {
        if (!isEditorCamera || textComponent->getRenderMode() == TextRenderMode::WORLD_SPACE) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_DEPTH_TEST);
            
            if (textComponent->getRenderMode() == TextRenderMode::WORLD_SPACE) {
                textComponent->renderWorldSpaceDirectly(worldTransform, viewMatrix, projectionMatrix);
            } else {
                textComponent->renderScreenSpaceDirectly();
            }
            
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
        // Render camera gizmo
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
        
        // Render camera frustum
        if (cameraComponent->getShowFrustum()) {
            auto frustumMesh = cameraComponent->getFrustumMesh();
            auto frustumMaterial = cameraComponent->getFrustumMaterial();
            
            if (frustumMesh && frustumMaterial) {
                // Store OpenGL state
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
#endif
    
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        auto child = node->getChild(i);
        if (child) {
            renderNodeDirectly(child, worldTransform, viewMatrix, projectionMatrix, isEditorCamera);
        }
    }
}

void EditorSystem::renderPhysicsDebugShapes(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
#ifdef EDITOR_BUILD
    auto& physicsManager = PhysicsManager::getInstance();
    if (!physicsManager.isDebugDrawEnabled() || !activeScene) return;
    
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

bool EditorSystem::saveSceneToFile(const std::string& filepath) {
    if (!activeScene) {
        std::cerr << "No active scene to save" << std::endl;
        return false;
    }
    
    return SceneSerializer::saveSceneToFile(activeScene, filepath);
}

bool EditorSystem::loadSceneFromFile(const std::string& filepath) {
    auto loadedScene = SceneSerializer::loadSceneFromFile(filepath);
    if (loadedScene) {
        setActiveScene(loadedScene);
        return true;
    }
    return false;
}

void EditorSystem::createNewScene() {
    createDefaultScene();
    clearSelection();
}

} // namespace GameEngine

#endif // LINUX_BUILD