#include "Components/CameraComponent.h"
#include "Scene/SceneNode.h"
#include "Core/Engine.h"
#include "Input/InputManager.h"
#include "Core/Time.h"
#include <iostream>

#ifdef EDITOR_BUILD
    #include "imgui.h"
    #include "Rendering/Mesh.h"
    #include "Rendering/Material.h"
    #include "Rendering/Shader.h"
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
    , yaw(-90.0f)
    , pitch(0.0f)
    , movementSpeed(5.0f)
    , mouseSensitivity(0.1f)
    , projectionMatrix(1.0f)
    , projectionDirty(true)
#ifdef EDITOR_BUILD
    , showGizmo(false)
    , showFrustum(true)
    , gizmoMesh(nullptr)
    , gizmoMaterial(nullptr)
    , frustumMesh(nullptr)
    , frustumMaterial(nullptr)
#endif
{
#ifdef EDITOR_BUILD
    // Create gizmo in constructor (doesn't need owner)
    createGizmo();
    // Frustum will be created when owner is set and updateFrustumMesh() is called
#endif
}

CameraComponent::~CameraComponent() {
    // Shared pointers will automatically clean up, no need to explicitly reset
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
        
        ImGui::Separator();
        if (ImGui::Checkbox("Show Gizmo", &showGizmo)) {
            if (showGizmo && !gizmoMesh) {
                createGizmo();
            }
        }
        if (ImGui::Checkbox("Show Frustum", &showFrustum)) {
            if (showFrustum) {
                updateFrustumMesh();
            } else {
                frustumMesh.reset();
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
    
#ifdef EDITOR_BUILD
    updateFrustumMesh();
#endif
}

#ifdef EDITOR_BUILD
void CameraComponent::createGizmo() {
    // Create a simple pyramid shape to represent the camera
    // The pyramid points forward (negative Z in camera space)
    std::vector<glm::vec3> vertices = {
        // Base of pyramid (near plane)
        {-0.2f, -0.15f, -0.3f},  // 0: bottom-left
        { 0.2f, -0.15f, -0.3f},  // 1: bottom-right
        { 0.2f,  0.15f, -0.3f},  // 2: top-right
        {-0.2f,  0.15f, -0.3f},  // 3: top-left
        // Tip of pyramid (camera position)
        { 0.0f,  0.0f,  0.0f}    // 4: tip
    };
    
    std::vector<unsigned int> indices = {
        // Base
        0, 1, 2, 2, 3, 0,
        // Sides
        0, 4, 1,
        1, 4, 2,
        2, 4, 3,
        3, 4, 0
    };
    
    gizmoMesh = std::make_shared<Mesh>();
    
    std::vector<Vertex> meshVertices;
    for (const auto& pos : vertices) {
        Vertex vertex;
        vertex.position = pos;
        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        vertex.texCoords = glm::vec2(0.0f, 0.0f);
        meshVertices.push_back(vertex);
    }
    
    gizmoMesh->setVertices(meshVertices);
    gizmoMesh->setIndices(indices);
    gizmoMesh->upload();
    
    gizmoMaterial = std::make_shared<Material>();
    gizmoMaterial->setColor(glm::vec3(0.2f, 0.8f, 0.2f)); // Green color for camera
    gizmoMaterial->setShader(Shader::getDefaultShader());
}

void CameraComponent::updateGizmo() {
    // Gizmo doesn't need updating based on camera properties
    // It's just a visual indicator
}

void CameraComponent::calculateFrustumCorners(std::vector<glm::vec3>& corners) const {
    corners.clear();
    corners.resize(8);
    
    if (!owner) return;
    
    // Calculate frustum corners in camera local space (camera looks down -Z)
    // The world transform will position them correctly
    if (projectionType == ProjectionType::PERSPECTIVE) {
        float tanHalfFOV = tan(glm::radians(fov * 0.5f));
        float nearHeight = nearPlane * tanHalfFOV;
        float nearWidth = nearHeight * aspectRatio;
        float farHeight = farPlane * tanHalfFOV;
        float farWidth = farHeight * aspectRatio;
        
        // Near plane corners (in camera local space, camera at origin looking down -Z)
        corners[0] = glm::vec3(-nearWidth, -nearHeight, -nearPlane); // bottom-left
        corners[1] = glm::vec3( nearWidth, -nearHeight, -nearPlane); // bottom-right
        corners[2] = glm::vec3( nearWidth,  nearHeight, -nearPlane); // top-right
        corners[3] = glm::vec3(-nearWidth,  nearHeight, -nearPlane); // top-left
        
        // Far plane corners
        corners[4] = glm::vec3(-farWidth, -farHeight, -farPlane); // bottom-left
        corners[5] = glm::vec3( farWidth, -farHeight, -farPlane); // bottom-right
        corners[6] = glm::vec3( farWidth,  farHeight, -farPlane); // top-right
        corners[7] = glm::vec3(-farWidth,  farHeight, -farPlane); // top-left
    } else {
        // Orthographic projection
        float halfWidth = orthographicSize * aspectRatio * 0.5f;
        float halfHeight = orthographicSize * 0.5f;
        
        // Near plane corners
        corners[0] = glm::vec3(-halfWidth, -halfHeight, -nearPlane); // bottom-left
        corners[1] = glm::vec3( halfWidth, -halfHeight, -nearPlane); // bottom-right
        corners[2] = glm::vec3( halfWidth,  halfHeight, -nearPlane); // top-right
        corners[3] = glm::vec3(-halfWidth,  halfHeight, -nearPlane); // top-left
        
        // Far plane corners
        corners[4] = glm::vec3(-halfWidth, -halfHeight, -farPlane); // bottom-left
        corners[5] = glm::vec3( halfWidth, -halfHeight, -farPlane); // bottom-right
        corners[6] = glm::vec3( halfWidth,  halfHeight, -farPlane); // top-right
        corners[7] = glm::vec3(-halfWidth,  halfHeight, -farPlane); // top-left
    }
}

void CameraComponent::createFrustumMesh() {
    updateFrustumMesh();
}

void CameraComponent::updateFrustumMesh() {
    if (!showFrustum || !owner) {
        frustumMesh.reset();
        return;
    }
    
    std::vector<glm::vec3> corners;
    calculateFrustumCorners(corners);
    
    if (corners.size() != 8) {
        frustumMesh.reset();
        return;
    }
    
    // Corners are already in camera local space, so we can use them directly
    std::vector<Vertex> vertices;
    for (const auto& corner : corners) {
        Vertex vertex;
        vertex.position = corner;
        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        vertex.texCoords = glm::vec2(0.0f, 0.0f);
        vertices.push_back(vertex);
    }
    
    // Create wireframe indices for frustum
    std::vector<unsigned int> indices = {
        // Near plane
        0, 1, 1, 2, 2, 3, 3, 0,
        // Far plane
        4, 5, 5, 6, 6, 7, 7, 4,
        // Connecting edges
        0, 4, 1, 5, 2, 6, 3, 7
    };
    
    // Reset old mesh first to ensure proper cleanup
    frustumMesh.reset();
    
    frustumMesh = std::make_shared<Mesh>();
    frustumMesh->setVertices(vertices);
    frustumMesh->setIndices(indices);
    frustumMesh->setRenderMode(GL_LINES);
    frustumMesh->upload();
    
    if (!frustumMaterial) {
        frustumMaterial = std::make_shared<Material>();
        frustumMaterial->setColor(glm::vec3(0.8f, 0.8f, 0.2f)); // Yellow color for frustum
        
        auto frustumShader = std::make_shared<Shader>();
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
        
        if (frustumShader->loadFromSource(vertexShaderSource, fragmentShaderSource)) {
            frustumMaterial->setShader(frustumShader);
        } else {
            frustumMaterial->setShader(Shader::getDefaultShader());
        }
    }
}
#endif

} // namespace GameEngine
