#include "Components/LightComponent.h"
#include "Scene/SceneNode.h"
#include "Core/Transform.h"
#include "Rendering/Renderer.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

#ifdef EDITOR_BUILD
    #include <imgui.h>
#endif

namespace GameEngine {

LightComponent::LightComponent()
    : type(LightType::POINT)
    , color(1.0f, 1.0f, 1.0f)
    , intensity(1.0f)
    , localPosition(0.0f, 0.0f, 0.0f)
    , localDirection(0.0f, -1.0f, 0.0f)
    , cutOff(glm::radians(12.5f))
    , outerCutOff(glm::radians(17.5f))
    , range(10.0f)
    , constant(1.0f)
    , linear(0.09f)
    , quadratic(0.032f)
    , showGizmo(true)
    , gizmoMesh(nullptr)
    , gizmoMaterial(nullptr)
{
}

LightComponent::~LightComponent() {
    destroy();
}

void LightComponent::start() {
    if (showGizmo) {
        createGizmo();
    }
}

void LightComponent::update(float deltaTime) {
    // Update light data based on transform
    if (owner) {
        Transform& transform = owner->getTransform();
        localPosition = transform.getPosition();
        localDirection = glm::normalize(transform.getRotation() * glm::vec3(0.0f, -1.0f, 0.0f));
    }
    
    // Update gizmo if needed
    if (showGizmo && gizmoMesh) {
        updateGizmo();
    }
}

void LightComponent::render(Renderer& renderer) {
    // Render gizmo in editor mode
#ifdef EDITOR_BUILD
    if (showGizmo && gizmoMesh && gizmoMaterial) {
        renderer.renderMesh(*gizmoMesh, *gizmoMaterial, owner->getWorldMatrix());
    }
#endif
}

void LightComponent::destroy() {
    gizmoMesh.reset();
    gizmoMaterial.reset();
}

void LightComponent::setType(LightType newType) {
    type = newType;
    
    // Adjust default values based on type
    switch (type) {
        case LightType::DIRECTIONAL:
            range = 1000.0f;  // Very large for directional
            constant = 1.0f;
            linear = 0.0f;
            quadratic = 0.0f;
            break;
        case LightType::POINT:
            range = 10.0f;
            constant = 1.0f;
            linear = 0.09f;
            quadratic = 0.032f;
            break;
        case LightType::SPOT:
            range = 10.0f;
            constant = 1.0f;
            linear = 0.09f;
            quadratic = 0.032f;
            break;
    }
}

glm::vec3 LightComponent::getPosition() const {
    if (owner) {
        glm::mat4 worldMatrix = owner->getWorldMatrix();
        return glm::vec3(worldMatrix[3]);
    }
    return localPosition;
}

void LightComponent::setPosition(const glm::vec3& newPosition) {
    if (owner) {
        owner->getTransform().setPosition(newPosition);
    }
    localPosition = newPosition;
}

glm::vec3 LightComponent::getDirection() const {
    if (owner) {
        glm::mat4 worldMatrix = owner->getWorldMatrix();
        glm::mat3 worldRotation = glm::mat3(worldMatrix);
        return glm::normalize(worldRotation * glm::vec3(0.0f, -1.0f, 0.0f));
    }
    return localDirection;
}

void LightComponent::setDirection(const glm::vec3& newDirection) {
    glm::vec3 normalizedDir = glm::normalize(newDirection);
    if (owner) {
        // Calculate rotation to point in the specified direction
        glm::vec3 defaultDir(0.0f, -1.0f, 0.0f);
        if (glm::length(normalizedDir - defaultDir) > 0.001f) {
            float angle = glm::acos(glm::dot(defaultDir, normalizedDir));
            glm::vec3 axis = glm::cross(defaultDir, normalizedDir);
            if (glm::length(axis) > 0.001f) {
                axis = glm::normalize(axis);
                // Manual quaternion construction for C++11 compatibility
                float halfAngle = angle * 0.5f;
                float sinHalfAngle = glm::sin(halfAngle);
                glm::quat rotation(glm::cos(halfAngle), axis.x * sinHalfAngle, axis.y * sinHalfAngle, axis.z * sinHalfAngle);
                owner->getTransform().setRotation(rotation);
            }
        }
    }
    localDirection = normalizedDir;
}

void LightComponent::createGizmo() {
    // Create a simple cube mesh for the light gizmo
    // This is a basic cube with 8 vertices and 12 triangles
    std::vector<glm::vec3> vertices = {
        // Front face
        {-0.1f, -0.1f,  0.1f},
        { 0.1f, -0.1f,  0.1f},
        { 0.1f,  0.1f,  0.1f},
        {-0.1f,  0.1f,  0.1f},
        // Back face
        {-0.1f, -0.1f, -0.1f},
        { 0.1f, -0.1f, -0.1f},
        { 0.1f,  0.1f, -0.1f},
        {-0.1f,  0.1f, -0.1f}
    };
    
    std::vector<unsigned int> indices = {
        // Front
        0, 1, 2, 2, 3, 0,
        // Back
        4, 7, 6, 6, 5, 4,
        // Left
        4, 0, 3, 3, 7, 4,
        // Right
        1, 5, 6, 6, 2, 1,
        // Top
        3, 2, 6, 6, 7, 3,
        // Bottom
        4, 5, 1, 1, 0, 4
    };
    
    // Create the gizmo mesh using the Mesh class
    gizmoMesh = std::make_shared<Mesh>();
    
    // Convert vertices to the proper format
    std::vector<Vertex> meshVertices;
    for (const auto& pos : vertices) {
        Vertex vertex;
        vertex.position = pos;
        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default normal
        vertex.texCoords = glm::vec2(0.0f, 0.0f); // Default UV
        meshVertices.push_back(vertex);
    }
    
    gizmoMesh->setVertices(meshVertices);
    gizmoMesh->setIndices(indices);
    gizmoMesh->upload();
    
    // Create and set up the gizmo material
    gizmoMaterial = std::make_shared<Material>();
    gizmoMaterial->setColor(color);
    
    // Use a simple shader for the gizmo
    gizmoMaterial->setShader(Shader::getDefaultShader());
}

void LightComponent::updateGizmo() {
    if (gizmoMaterial) {
        gizmoMaterial->setColor(color);
    }
}

LightComponent::LightData LightComponent::getLightData() const {
    LightData data;
    
    // Position (w component encodes light type)
    data.position = glm::vec4(getPosition(), static_cast<float>(type));
    
    // Direction and intensity
    data.direction = glm::vec4(getDirection(), intensity);
    
    // Color and range
    data.color = glm::vec4(color, range);
    
    // Parameters (cutOff, outerCutOff, constant, linear)
    data.params = glm::vec4(cutOff, outerCutOff, constant, linear);
    
    // Attenuation (quadratic and unused components)
    data.attenuation = glm::vec4(quadratic, 0.0f, 0.0f, 0.0f);
    
    return data;
}

void LightComponent::drawInspector() {
#ifdef EDITOR_BUILD
    // Light type
    const char* typeNames[] = {"Directional", "Point", "Spot"};
    int currentType = static_cast<int>(type);
    if (ImGui::Combo("Light Type", &currentType, typeNames, 3)) {
        setType(static_cast<LightType>(currentType));
    }
    
    // Color
    float colorArray[3] = {color.r, color.g, color.b};
    if (ImGui::ColorEdit3("Color", colorArray)) {
        color = glm::vec3(colorArray[0], colorArray[1], colorArray[2]);
    }
    
    // Intensity
    if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 10.0f)) {
        // Intensity updated
    }
    
    // Position (only for point and spot lights)
    if (type != LightType::DIRECTIONAL) {
        float posArray[3] = {localPosition.x, localPosition.y, localPosition.z};
        if (ImGui::DragFloat3("Position", posArray, 0.1f)) {
            setPosition(glm::vec3(posArray[0], posArray[1], posArray[2]));
        }
    }
    
    // Direction (for directional and spot lights)
    if (type != LightType::POINT) {
        float dirArray[3] = {localDirection.x, localDirection.y, localDirection.z};
        if (ImGui::DragFloat3("Direction", dirArray, 0.1f)) {
            setDirection(glm::vec3(dirArray[0], dirArray[1], dirArray[2]));
        }
    }
    
    // Spotlight specific properties
    if (type == LightType::SPOT) {
        float cutOffDegrees = glm::degrees(cutOff);
        if (ImGui::SliderFloat("Cut Off (degrees)", &cutOffDegrees, 0.0f, 90.0f)) {
            cutOff = glm::radians(cutOffDegrees);
        }
        
        float outerCutOffDegrees = glm::degrees(outerCutOff);
        if (ImGui::SliderFloat("Outer Cut Off (degrees)", &outerCutOffDegrees, 0.0f, 90.0f)) {
            outerCutOff = glm::radians(outerCutOffDegrees);
        }
    }
    
    // Range (for point and spot lights)
    if (type != LightType::DIRECTIONAL) {
        if (ImGui::SliderFloat("Range", &range, 0.1f, 100.0f)) {
            // Range updated
        }
    }
    
    // Attenuation (for point and spot lights)
    if (type != LightType::DIRECTIONAL) {
        if (ImGui::SliderFloat("Constant", &constant, 0.0f, 2.0f)) {
            // Constant updated
        }
        if (ImGui::SliderFloat("Linear", &linear, 0.0f, 1.0f)) {
            // Linear updated
        }
        if (ImGui::SliderFloat("Quadratic", &quadratic, 0.0f, 1.0f)) {
            // Quadratic updated
        }
    }
    
    // Gizmo visibility
    ImGui::Checkbox("Show Gizmo", &showGizmo);
#endif
}

} // namespace GameEngine
