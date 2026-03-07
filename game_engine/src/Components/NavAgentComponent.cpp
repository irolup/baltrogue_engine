#include "Components/NavAgentComponent.h"
#include "Navigation/NavGrid.h"
#include "Navigation/NavGridRegistry.h"
#include "Components/NavVolumeComponent.h"
#include "Scene/SceneNode.h"
#include "Scene/Scene.h"
#include "Core/Engine.h"
#include "Components/PhysicsComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <limits>
#include <functional>
#include <cstring>

#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

NavAgentComponent::NavAgentComponent()
    : speed_(5.0f)
    , pathIndex_(0)
    , velocity_(0.0f)
    , pathFinishedThisFrame_(false)
    , pathJustSet_(false)
    , turnSpeed_(8.0f)
{}

void NavAgentComponent::update(float deltaTime) {
    PhysicsComponent* childPhys = findChildPhysicsComponent();
    if (!childPhys && owner && owner->getParent())
        childPhys = findSiblingPhysicsComponent();

    const bool bodyCanMove = childPhys && (childPhys->getBodyType() == PhysicsBodyType::DYNAMIC ||
                                           childPhys->getBodyType() == PhysicsBodyType::KINEMATIC);
    if (owner && bodyCanMove && childPhys->getOwner()) {
        glm::vec3 bodyWorldPos = childPhys->getWorldPosition();
        SceneNode* parent = owner->getParent();
        if (parent) {
            glm::mat4 parentWorldInv = glm::inverse(parent->getWorldMatrix());
            glm::vec3 localPos = glm::vec3(parentWorldInv * glm::vec4(bodyWorldPos, 1.0f));
            owner->getTransform().setPosition(localPos);
        } else {
            owner->getTransform().setPosition(bodyWorldPos);
        }
    }
    if (owner && !childPhys)
        owner->getTransform().setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    if (!owner || path_.empty() || pathIndex_ >= (int)path_.size()) {
        velocity_ = glm::vec3(0.0f);
        if (childPhys && childPhys->getBodyType() == PhysicsBodyType::DYNAMIC)
            childPhys->setLinearVelocity(glm::vec3(0.0f));
        return;
    }

    glm::vec3 currentPos = getPosition();
    glm::vec3 target = path_[(size_t)pathIndex_];
    target.y = currentPos.y;
    glm::vec3 toTarget = target - currentPos;
    toTarget.y = 0.0f;
    float dist = glm::length(toTarget);

    if (dist < 0.05f) {
        pathIndex_++;
        if (pathIndex_ >= (int)path_.size()) {
            pathFinishedThisFrame_ = true;
            velocity_ = glm::vec3(0.0f);
            if (childPhys && childPhys->getBodyType() == PhysicsBodyType::DYNAMIC)
                childPhys->setLinearVelocity(glm::vec3(0.0f));
            return;
        }
        target = path_[(size_t)pathIndex_];
        target.y = currentPos.y;
        toTarget = target - currentPos;
        toTarget.y = 0.0f;
        dist = glm::length(toTarget);
    }

    if (dist < 1e-6f) {
        velocity_ = glm::vec3(0.0f);
        return;
    }

    glm::vec3 dir = toTarget / dist;
    velocity_ = dir * speed_;
    velocity_.y = 0.0f;

    if (!pathJustSet_ && (dir.x != 0.0f || dir.z != 0.0f)) {
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 forward = dir;
        glm::vec3 right = glm::normalize(glm::cross(forward, up));
        glm::vec3 actualUp = glm::cross(right, forward);
        glm::mat3 rotMat;
        rotMat[0] = right;
        rotMat[1] = actualUp;
        rotMat[2] = -forward;
        glm::quat targetRot = glm::quat_cast(rotMat);
        const glm::quat& curRot = owner->getTransform().getRotation();
        if (glm::dot(targetRot, curRot) < 0.0f)
            targetRot = -targetRot;
        float t = std::min(1.0f, turnSpeed_ * deltaTime);
        owner->getTransform().setRotation(glm::slerp(curRot, targetRot, t));
    }
    pathJustSet_ = false;

    if (childPhys && childPhys->getBodyType() == PhysicsBodyType::DYNAMIC) {
        glm::vec3 desired(dir.x * speed_, 0.0f, dir.z * speed_);
        glm::vec3 current = childPhys->getLinearVelocity();
        const float blend = 0.3f;
        glm::vec3 vel;
        vel.x = current.x * (1.0f - blend) + desired.x * blend;
        vel.z = current.z * (1.0f - blend) + desired.z * blend;
        vel.y = current.y;
        childPhys->setLinearVelocity(vel);
        return;
    }

    float move = speed_ * deltaTime;
    glm::vec3 nextPosWorld;
    if (move >= dist) {
        nextPosWorld = target;
        pathIndex_++;
        if (pathIndex_ >= (int)path_.size())
            pathFinishedThisFrame_ = true;
    } else {
        nextPosWorld = currentPos + dir * move;
        nextPosWorld.y = currentPos.y;
    }

    SceneNode* parent = owner->getParent();
    if (parent) {
        glm::mat4 parentWorldInv = glm::inverse(parent->getWorldMatrix());
        glm::vec3 localPos = glm::vec3(parentWorldInv * glm::vec4(nextPosWorld, 1.0f));
        owner->getTransform().setPosition(localPos);
    } else {
        owner->getTransform().setPosition(nextPosWorld);
    }
}

void NavAgentComponent::setDestination(const glm::vec3& worldPos) {
    pathFinishedThisFrame_ = false;
    pathJustSet_ = true;
    glm::vec3 start = getPosition();
    NavGrid* grid = getGridForAgent();
    if (!grid) {
        path_.clear();
        pathIndex_ = 0;
        return;
    }
    path_ = grid->findPath(start, worldPos, true);
    pathIndex_ = 0;
    if (path_.size() > 1)
        pathIndex_ = 1;
    else if (path_.size() <= 1)
        path_.clear();
}

void NavAgentComponent::setFollowPath(const std::vector<glm::vec3>& path) {
    pathFinishedThisFrame_ = false;
    pathJustSet_ = true;
    path_ = path;
    pathIndex_ = path_.empty() ? 0 : 1;
}

void NavAgentComponent::clearDestination() {
    path_.clear();
    pathIndex_ = 0;
    pathFinishedThisFrame_ = false;
    velocity_ = glm::vec3(0.0f);
}

NavGrid* NavAgentComponent::getGridForAgent() const {
#ifdef EDITOR_BUILD
    auto scene = GetEngine().isEditorMode() ? GetEngine().getEditor().getActiveScene() : GetEngine().getSceneManager().getCurrentScene();
#else
    auto scene = GetEngine().getSceneManager().getCurrentScene();
#endif
    if (!scene) return NavGridRegistry::get().getFirstGrid();
    if (!assignedVolumeNodeName_.empty()) {
        auto node = scene->findNode(assignedVolumeNodeName_);
        if (node) {
            NavVolumeComponent* vol = node->getComponent<NavVolumeComponent>();
            if (vol && vol->getGrid()) return vol->getGrid();
        }
    }
    return NavGridRegistry::get().getFirstGrid();
}

bool NavAgentComponent::consumePathFinishedFlag() {
    bool v = pathFinishedThisFrame_;
    pathFinishedThisFrame_ = false;
    return v;
}

glm::vec3 NavAgentComponent::getPosition() const {
    if (!owner) return glm::vec3(0.0f);

    glm::mat4 world = owner->getWorldMatrix();
    return glm::vec3(world[3]);
}

PhysicsComponent* NavAgentComponent::findChildPhysicsComponent() const {
    if (!owner) return nullptr;
    std::function<PhysicsComponent*(SceneNode*)> search = [&search](SceneNode* n) -> PhysicsComponent* {
        if (!n) return nullptr;
        PhysicsComponent* p = n->getComponent<PhysicsComponent>();
        if (p) return p;
        for (size_t i = 0; i < n->getChildCount(); ++i) {
            auto ch = n->getChild(i);
            if (ch) {
                PhysicsComponent* found = search(ch.get());
                if (found) return found;
            }
        }
        return nullptr;
    };
    for (size_t i = 0; i < owner->getChildCount(); ++i) {
        auto child = owner->getChild(i);
        if (child) {
            PhysicsComponent* p = search(child.get());
            if (p) return p;
        }
    }
    return nullptr;
}

PhysicsComponent* NavAgentComponent::findSiblingPhysicsComponent() const {
    if (!owner) return nullptr;
    SceneNode* parent = owner->getParent();
    if (!parent) return nullptr;
    glm::vec3 ownerPos = getPosition();
    PhysicsComponent* best = nullptr;
    float bestDist = std::numeric_limits<float>::max();
    for (size_t i = 0; i < parent->getChildCount(); ++i) {
        auto sibling = parent->getChild(i);
        if (sibling.get() == owner) continue;
        if (!sibling) continue;
        PhysicsComponent* p = sibling->getComponent<PhysicsComponent>();
        if (!p) continue;
        glm::vec3 bodyPos = p->getWorldPosition();
        float d = glm::length(bodyPos - ownerPos);
        if (d < bestDist) {
            bestDist = d;
            best = p;
        }
    }
    return best;
}

void NavAgentComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::TreeNode("NavAgent")) {
        float s = speed_;
        if (ImGui::DragFloat("Speed", &s, 0.1f, 0.0f, 100.0f))
            setSpeed(s);
        float ts = turnSpeed_;
        if (ImGui::DragFloat("Turn speed", &ts, 0.5f, 0.1f, 50.0f))
            setTurnSpeed(ts);
        char buf[256];
        strncpy(buf, assignedVolumeNodeName_.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Nav Volume (node name)", buf, sizeof(buf)))
            setAssignedVolumeNodeName(std::string(buf));
        ImGui::SameLine();
        if (ImGui::Button("Pick...")) {
            ImGui::OpenPopup("nav_volume_pick");
        }
        if (ImGui::BeginPopup("nav_volume_pick")) {
            auto scene = GetEngine().getEditor().getActiveScene();
            if (scene && scene->getRootNode()) {
                std::function<void(const std::shared_ptr<SceneNode>&, int)> visit = [&](const std::shared_ptr<SceneNode>& node, int depth) {
                    if (!node || !node->isActive()) return;
                    if (node->getComponent<NavVolumeComponent>()) {
                        if (ImGui::Selectable(node->getName().c_str(), assignedVolumeNodeName_ == node->getName()))
                            setAssignedVolumeNodeName(node->getName());
                    }
                    for (size_t i = 0; i < node->getChildCount(); ++i)
                        visit(node->getChild(i), depth + 1);
                };
                visit(scene->getRootNode(), 0);
            }
            ImGui::EndPopup();
        }
        ImGui::Text("Path points: %d", (int)path_.size());
        ImGui::Text("Path index: %d", pathIndex_);
        ImGui::TreePop();
    }
#endif
}

} // namespace GameEngine
