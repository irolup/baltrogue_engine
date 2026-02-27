#include "Components/RaycastComponent.h"
#include "Physics/PhysicsManager.h"
#include "Scene/SceneNode.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include <glm/gtc/matrix_transform.hpp>

#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

#if defined(LINUX_BUILD)
#include <GL/gl.h>
#endif

namespace GameEngine {

RaycastComponent::RaycastComponent()
    : from_(0.0f, 0.0f, 0.0f)
    , to_(0.0f, 0.0f, -1.0f)
    , collisionMask_(-1)
    , showDebugLine_(true)
{
}

void RaycastComponent::setFrom(const glm::vec3& from) {
    from_ = from;
}

void RaycastComponent::setTo(const glm::vec3& to) {
    to_ = to;
}

void RaycastComponent::setCollisionMask(int mask) {
    collisionMask_ = mask;
}

void RaycastComponent::setShowDebugLine(bool show) {
    showDebugLine_ = show;
}

glm::vec3 RaycastComponent::getWorldFrom() const {
    if (!owner) return from_;
    glm::mat4 world = owner->getWorldMatrix();
    return glm::vec3(world * glm::vec4(from_, 1.0f));
}

glm::vec3 RaycastComponent::getWorldTo() const {
    if (!owner) return to_;
    glm::mat4 world = owner->getWorldMatrix();
    return glm::vec3(world * glm::vec4(to_, 1.0f));
}

bool RaycastComponent::performRaycast(bool& hit, std::string& hitNodeName, glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance) const {
    glm::vec3 wFrom = getWorldFrom();
    glm::vec3 wTo = getWorldTo();
    return PhysicsManager::getInstance().raycastFromTo(wFrom, wTo, collisionMask_,
        hit, hitNodeName, hitPoint, hitNormal, hitDistance);
}

void RaycastComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::TreeNode("Raycast")) {
        ImGui::TextWrapped("From and To are in this node's local space (affected by node position/rotation).");
        glm::vec3 from = from_;
        if (ImGui::DragFloat3("From", &from.x, 0.1f)) {
            setFrom(from);
        }
        glm::vec3 to = to_;
        if (ImGui::DragFloat3("To", &to.x, 0.1f)) {
            setTo(to);
        }
        int mask = collisionMask_;
        if (ImGui::InputInt("Collision Mask", &mask)) {
            setCollisionMask(mask);
        }
        ImGui::TextWrapped("Mask -1 = all groups. Otherwise use same bits as physics collision layers.");
        bool showDebug = showDebugLine_;
        if (ImGui::Checkbox("Show Debug Line (Editor)", &showDebug)) {
            setShowDebugLine(showDebug);
        }
        ImGui::TreePop();
    }
#endif
}

#if defined(EDITOR_BUILD) || defined(LINUX_BUILD)
void RaycastComponent::renderDebugLine(Material& debugMaterial, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!showDebugLine_ || !owner) return;
    glm::vec3 wFrom = getWorldFrom();
    glm::vec3 wTo = getWorldTo();
    glm::vec3 dir = wTo - wFrom;
    float L = glm::length(dir);
    if (L < 1e-6f) return;
    glm::vec3 dirN = dir / L;
    glm::vec3 axis = glm::cross(glm::vec3(0.0f, 0.0f, -1.0f), dirN);
    float angle = acos(glm::clamp(-dirN.z, -1.0f, 1.0f));
    glm::mat4 R = glm::mat4(1.0f);
    if (glm::length(axis) > 1e-6f) {
        R = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));
    } else if (dirN.z > 0.99f) {
        R = glm::rotate(glm::mat4(1.0f), 3.14159265f, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, L));
    glm::mat4 T = glm::translate(glm::mat4(1.0f), wFrom);
    glm::mat4 modelMatrix = T * R * S;
    std::shared_ptr<Mesh> lineMesh = Mesh::createLineSegment();
    if (!lineMesh) return;
    lineMesh->bind();
    debugMaterial.apply();
    auto shader = debugMaterial.getShader();
    if (shader) {
        shader->setMat4("modelMatrix", modelMatrix);
        shader->setMat4("viewMatrix", viewMatrix);
        shader->setMat4("projectionMatrix", projectionMatrix);
    }
    lineMesh->draw();
    lineMesh->unbind();
}
#endif

} // namespace GameEngine
