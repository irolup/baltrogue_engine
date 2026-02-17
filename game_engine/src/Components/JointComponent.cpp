#include "Components/JointComponent.h"
#include "Components/PhysicsComponent.h"
#include "Physics/PhysicsManager.h"
#include "Scene/SceneNode.h"
#include "Core/Engine.h"
#include "Core/Transform.h"
#include <glm/gtc/quaternion.hpp>

#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btFixedConstraint.h>

#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

JointComponent::JointComponent()
    : jointType(JointType::FIXED)
    , pivotA(0.0f)
    , pivotB(0.0f)
    , enabled(true)
    , constraint(nullptr)
    , constraintCreatePending(false)
{
}

JointComponent::~JointComponent() {
    destroyConstraint();
}

void JointComponent::start() {
    if (!enabled || bodyAName.empty() || bodyBName.empty()) return;
    createConstraint();
    if (!constraint) constraintCreatePending = true;
}

void JointComponent::update(float deltaTime) {
    (void)deltaTime;
    if (constraintCreatePending && enabled && !bodyAName.empty() && !bodyBName.empty()) {
        createConstraint();
        if (constraint) constraintCreatePending = false;
    }
}

void JointComponent::destroy() {
    destroyConstraint();
}

void JointComponent::setBodyA(const std::string& nodeName) {
    if (constraint) {
        destroyConstraint();
    }
    bodyAName = nodeName;
}

void JointComponent::setBodyB(const std::string& nodeName) {
    if (constraint) {
        destroyConstraint();
    }
    bodyBName = nodeName;
}

void JointComponent::setJointType(JointType type) {
    jointType = type;
}

void JointComponent::setPivotA(const glm::vec3& pivot) {
    pivotA = pivot;
}

void JointComponent::setPivotB(const glm::vec3& pivot) {
    pivotB = pivot;
}

void JointComponent::setEnabled(bool enabled) {
    if (this->enabled != enabled) {
        this->enabled = enabled;
        if (!enabled && constraint) {
            destroyConstraint();
        } else if (enabled && owner && !bodyAName.empty() && !bodyBName.empty()) {
            createConstraint();
        }
    }
}

void JointComponent::createConstraint() {
    if (constraint) return;
    if (!owner) return;
    
    auto activeScene = GetEngine().getSceneManager().getCurrentScene();
    if (!activeScene) return;
    
    auto nodeA = activeScene->findNode(bodyAName);
    auto nodeB = activeScene->findNode(bodyBName);
    if (!nodeA || !nodeB) return;
    
    PhysicsComponent* physA = nodeA->getComponent<PhysicsComponent>();
    PhysicsComponent* physB = nodeB->getComponent<PhysicsComponent>();
    if (!physA || !physB) return;
    
    btRigidBody* rbA = physA->getRigidBody();
    btRigidBody* rbB = physB->getRigidBody();
    if (!rbA || !rbB) return;
    
    btTransform frameInA, frameInB;
    frameInA.setIdentity();
    frameInA.setOrigin(btVector3(pivotA.x, pivotA.y, pivotA.z));
    frameInB.setIdentity();
    frameInB.setOrigin(btVector3(pivotB.x, pivotB.y, pivotB.z));
    
    if (jointType == JointType::FIXED) {
        btFixedConstraint* fixed = new btFixedConstraint(*rbA, *rbB, frameInA, frameInB);
        constraint = fixed;
        PhysicsManager::getInstance().addConstraint(constraint, true);
        rbA->activate();
        rbB->activate();
    }
}

void JointComponent::destroyConstraint() {
    if (constraint) {
        PhysicsManager::getInstance().removeConstraint(constraint);
        delete constraint;
        constraint = nullptr;
    }
}

void JointComponent::drawInspector() {
#ifdef EDITOR_BUILD
    char bufA[256] = {0};
    char bufB[256] = {0};
    if (bodyAName.size() < sizeof(bufA)) std::strcpy(bufA, bodyAName.c_str());
    if (bodyBName.size() < sizeof(bufB)) std::strcpy(bufB, bodyBName.c_str());
    
    if (ImGui::InputText("Body A", bufA, sizeof(bufA))) bodyAName = bufA;
    if (ImGui::InputText("Body B", bufB, sizeof(bufB))) bodyBName = bufB;
    
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::DragFloat3("Pivot A", &pivotA.x, 0.01f);
    ImGui::DragFloat3("Pivot B", &pivotB.x, 0.01f);
#endif
}

} // namespace GameEngine
