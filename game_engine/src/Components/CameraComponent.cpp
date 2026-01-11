#include "Components/CameraComponent.h"
#include "Scene/SceneNode.h"
#include "Core/Engine.h"
#include "Input/InputManager.h"
#include "Core/Time.h"
#include <iostream>

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

CameraComponent::CameraComponent()
    : projectionType(ProjectionType::PERSPECTIVE)
    , fov(45.0f)
    , aspectRatio(16.0f / 9.0f)
    , nearPlane(0.1f)
    , farPlane(1000.0f)
    , orthographicSize(5.0f)
    , viewport(0.0f, 0.0f, 1.0f, 1.0f)
    , isActiveCamera(false)
    , controlsEnabled(false)
    , projectionMatrix(1.0f)
    , projectionDirty(true)
    , yaw(-90.0f)
    , pitch(0.0f)
    , movementSpeed(5.0f)
    , mouseSensitivity(0.1f)
{
}

CameraComponent::~CameraComponent() {
}

void CameraComponent::update(float deltaTime) {
    if (!isActiveCamera || !owner) return;
    
    handleKeyboardInput(deltaTime);
    handleMouseInput();
    
    if (projectionDirty) {
        updateProjection();
    }
}

void CameraComponent::handleKeyboardInput(float deltaTime) {
    if (!controlsEnabled) {
        return;
    }
    
    auto& inputManager = GetEngine().getInputManager();
    float moveDistance = movementSpeed * deltaTime;
    
#ifdef LINUX_BUILD
    if (inputManager.isKeyHeld(GLFW_KEY_W)) {
        moveForward(moveDistance);
    }
    if (inputManager.isKeyHeld(GLFW_KEY_S)) {
        moveForward(-moveDistance);
    }
    
    if (inputManager.isKeyHeld(GLFW_KEY_A)) {
        moveRight(-moveDistance);
    }
    if (inputManager.isKeyHeld(GLFW_KEY_D)) {
        moveRight(moveDistance);
    }
    
    if (inputManager.isKeyHeld(GLFW_KEY_SPACE)) {
        moveUp(moveDistance);
    }
    if (inputManager.isKeyHeld(GLFW_KEY_LEFT_SHIFT)) {
        moveUp(-moveDistance);
    }
#endif

#ifdef VITA_BUILD
    glm::vec2 leftStick = inputManager.getLeftStick();
    
    if (leftStick.y > 0.1f) {
        moveForward(-moveDistance * leftStick.y);
    } else if (leftStick.y < -0.1f) {
        moveForward(-moveDistance * leftStick.y);
    }
    
    if (leftStick.x > 0.1f) {
        moveRight(moveDistance * leftStick.x);
    } else if (leftStick.x < -0.1f) {
        moveRight(moveDistance * leftStick.x);
    }
#endif
}

void CameraComponent::handleMouseInput() {
    if (!controlsEnabled) {
        return;
    }
    
    auto& inputManager = GetEngine().getInputManager();
    
#ifdef LINUX_BUILD
    static double lastX = 0, lastY = 0;
    glm::vec2 mousePos = inputManager.getMousePosition();
    double xpos = mousePos.x, ypos = mousePos.y;
    
    if (lastX == 0 && lastY == 0) {
        lastX = xpos;
        lastY = ypos;
        return;
    }
    
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    
    if (inputManager.isMouseButtonHeld(GLFW_MOUSE_BUTTON_RIGHT)) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;
        
        yaw += xoffset;
        pitch += yoffset;
        
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        
        updateRotation();
    }
#endif

#ifdef VITA_BUILD
    glm::vec2 rightStick = inputManager.getRightStick();
    
    if (glm::length(rightStick) > 0.1f) {
        float xoffset = rightStick.x * mouseSensitivity * 2.0f;
        float yoffset = rightStick.y * mouseSensitivity * 2.0f;
        
        yaw += xoffset;
        pitch -= yoffset;
        
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        
        updateRotation();
    }
#endif
}

void CameraComponent::updateRotation() {
    if (!owner) return;
    
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction = glm::normalize(direction);
    
    glm::vec3 defaultDir(0.0f, 0.0f, -1.0f);
    float angle = glm::acos(glm::dot(defaultDir, direction));
    glm::vec3 axis = glm::cross(defaultDir, direction);
    
    if (glm::length(axis) > 0.001f) {
        axis = glm::normalize(axis);
        glm::quat rotation(glm::cos(angle * 0.5f), axis.x * glm::sin(angle * 0.5f), 
                          axis.y * glm::sin(angle * 0.5f), axis.z * glm::sin(angle * 0.5f));
        owner->getTransform().setRotation(rotation);
    }
}

void CameraComponent::moveForward(float distance) {
    if (!owner) return;
    
    glm::vec3 forward = getForward();
    glm::vec3 position = owner->getTransform().getPosition();
    position += forward * distance;
    owner->getTransform().setPosition(position);
}

void CameraComponent::moveRight(float distance) {
    if (!owner) return;
    
    glm::vec3 right = getRight();
    glm::vec3 position = owner->getTransform().getPosition();
    position += right * distance;
    owner->getTransform().setPosition(position);
}

void CameraComponent::moveUp(float distance) {
    if (!owner) return;
    
    glm::vec3 up = getUp();
    glm::vec3 position = owner->getTransform().getPosition();
    position += up * distance;
    owner->getTransform().setPosition(position);
}

void CameraComponent::rotate(float yawDelta, float pitchDelta) {
    yaw += yawDelta;
    pitch += pitchDelta;
    
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    
    updateRotation();
}

glm::mat4 CameraComponent::getViewMatrix() {
    if (!owner) return glm::mat4(1.0f);
    
    if (!owner->isActive() || !owner->isVisible()) {
        return glm::mat4(1.0f);
    }
    
    glm::mat4 worldMatrix = owner->getWorldMatrix();
    return glm::inverse(worldMatrix);
}

glm::mat4 CameraComponent::getProjectionMatrix() {
    if (projectionDirty) {
        updateProjection();
    }
    return projectionMatrix;
}

glm::mat4 CameraComponent::getViewProjectionMatrix() {
    return getProjectionMatrix() * getViewMatrix();
}

glm::vec3 CameraComponent::getForward() const {
    if (!owner) return glm::vec3(0, 0, -1);
    return owner->getTransform().getForward();
}

glm::vec3 CameraComponent::getRight() const {
    if (!owner) return glm::vec3(1, 0, 0);
    return owner->getTransform().getRight();
}

glm::vec3 CameraComponent::getUp() const {
    if (!owner) return glm::vec3(0, 1, 0);
    return owner->getTransform().getUp();
}

void CameraComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (owner) {
            auto scene = GetEngine().getSceneManager().getCurrentScene();
            bool isCurrentlyActive = false;
            if (scene) {
                auto activeCameraNode = scene->getActiveCamera();
                if (activeCameraNode && activeCameraNode->getComponent<CameraComponent>() == this) {
                    isCurrentlyActive = true;
                }
            }
            if (!isCurrentlyActive) {
                isCurrentlyActive = isActiveCamera;
            }
            
            if (isCurrentlyActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.1f, 1.0f));
            }
            
            if (ImGui::Button(isCurrentlyActive ? "Active Camera" : "Set as Active Camera")) {
                auto scene = GetEngine().getSceneManager().getCurrentScene();
                if (scene && owner) {
                    std::function<std::shared_ptr<SceneNode>(std::shared_ptr<SceneNode>)> findNodeWithComponent;
                    CameraComponent* thisPtr = this;
                    findNodeWithComponent = [thisPtr, &findNodeWithComponent](std::shared_ptr<SceneNode> node) -> std::shared_ptr<SceneNode> {
                        if (!node) return nullptr;
                        
                        if (node->getComponent<CameraComponent>() == thisPtr) {
                            return node;
                        }
                        
                        for (size_t i = 0; i < node->getChildCount(); ++i) {
                            auto result = findNodeWithComponent(node->getChild(i));
                            if (result) return result;
                        }
                        
                        return nullptr;
                    };
                    
                    auto rootNode = scene->getRootNode();
                    if (rootNode) {
                        auto cameraNode = findNodeWithComponent(rootNode);
                        if (cameraNode) {
                            scene->setActiveCamera(cameraNode);
                        } else {
                            std::cout << "CameraComponent::drawInspector: Could not find camera node in scene!" << std::endl;
                        }
                    }
                }
            }
            
            if (isCurrentlyActive) {
                ImGui::PopStyleColor(3);
            }
            
            ImGui::SameLine();
            if (isCurrentlyActive) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "✓");
            }
            ImGui::Separator();
        }
        
        // Projection type
        const char* projectionTypes[] = { "Perspective", "Orthographic" };
        int currentProjection = (int)projectionType;
        if (ImGui::Combo("Projection", &currentProjection, projectionTypes, 2)) {
            setProjectionType((ProjectionType)currentProjection);
        }
        
        if (projectionType == ProjectionType::PERSPECTIVE) {
            if (ImGui::SliderFloat("Field of View", &fov, 10.0f, 120.0f)) {
                updateProjection();
            }
        } else {
            if (ImGui::DragFloat("Orthographic Size", &orthographicSize, 0.1f, 0.1f, 100.0f)) {
                updateProjection();
            }
        }
        
        if (ImGui::DragFloat("Near Plane", &nearPlane, 0.01f, 0.01f, farPlane - 0.01f)) {
                updateProjection();
        }
        
        if (ImGui::DragFloat("Far Plane", &farPlane, 1.0f, nearPlane + 0.01f, 10000.0f)) {
                updateProjection();
        }
        
        ImGui::Text("Aspect Ratio: %.2f", aspectRatio);
        
        if (ImGui::CollapsingHeader("Viewport")) {
            if (ImGui::DragFloat4("Viewport", &viewport.x, 0.01f, 0.0f, 1.0f)) {
            }
        }
    }
#endif
}

void CameraComponent::updateProjection() {
    if (projectionType == ProjectionType::PERSPECTIVE) {
        projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    } else {
        float halfWidth = orthographicSize * aspectRatio * 0.5f;
        float halfHeight = orthographicSize * 0.5f;
        projectionMatrix = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
    }
    
    projectionDirty = false;
}

} // namespace GameEngine
