#include "Components/PhysicsComponent.h"
#include "Platform/VitaMath.h"
#include "Physics/PhysicsManager.h"
#include "Scene/SceneNode.h"
#include "Core/Transform.h"
#include "Rendering/Mesh.h"
#include "Rendering/Renderer.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include <functional>

// Enable experimental GLM extensions for Vita builds
#define GLM_ENABLE_EXPERIMENTAL

// Additional glm includes for quaternion operations
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// Bullet includes
#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

// ImGui for editor
#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

PhysicsComponent::PhysicsComponent()
    : collisionShapeType(CollisionShapeType::BOX)
    , shapeDimensions(1.0f, 1.0f, 1.0f)
    , bodyType(PhysicsBodyType::STATIC)
    , mass(0.0f)
    , friction(0.5f)
    , restitution(0.0f)
    , linearDamping(0.0f)
    , angularDamping(0.0f)
    , rigidBody(nullptr)
    , collisionShape(nullptr)
    , motionState(nullptr)
    , isCollidingFlag(false)
    , showCollisionShape(true)  // Enable collision shape visualization by default
    , lastWorldTransform(1.0f)
    , destroyed(false)
{
}

PhysicsComponent::~PhysicsComponent() {
    if (!destroyed) {
        destroy();
    }
}

void PhysicsComponent::start() {
    PhysicsManager::getInstance().registerPhysicsComponent(this);
    
    if (owner) {
        lastWorldTransform = owner->getWorldMatrix();
    }
    
    createRigidBody();
}

void PhysicsComponent::update(float deltaTime) {
#ifdef EDITOR_BUILD
    return;
#else
    if (rigidBody && owner) {
        glm::mat4 currentWorldTransform = owner->getWorldMatrix();
        
        float transformDifference = glm::length(glm::vec3(currentWorldTransform[3]) - 
                                              glm::vec3(lastWorldTransform[3]));
        
        const float MIN_TRANSFORM_THRESHOLD = 0.001f;
        
        if (transformDifference > MIN_TRANSFORM_THRESHOLD) {
            syncTransformToPhysics();
            lastWorldTransform = currentWorldTransform;
        }
    }
#endif
}

void PhysicsComponent::destroy() {
    if (destroyed) {
        return;
    }
    
    destroyed = true;
    
    PhysicsManager::getInstance().unregisterPhysicsComponent(this);
    
    destroyRigidBody();
}

void PhysicsComponent::forceUpdateCollisionShape() {
    if (rigidBody && owner) {
        lastWorldTransform = owner->getWorldMatrix();
        syncTransformToPhysics();
    }
}

void PhysicsComponent::setCollisionShape(CollisionShapeType shapeType, const glm::vec3& dimensions) {
    collisionShapeType = shapeType;
    shapeDimensions = dimensions;
    
    if (rigidBody) {
        updateCollisionShape();
    }
}

void PhysicsComponent::setBodyType(PhysicsBodyType type) {
    bodyType = type;
    
    if (rigidBody) {
#ifdef EDITOR_BUILD
        int flags = btCollisionObject::CF_STATIC_OBJECT;
        rigidBody->setCollisionFlags(flags);
        
        rigidBody->setActivationState(DISABLE_SIMULATION);
        
        rigidBody->setLinearVelocity(btVector3(0, 0, 0));
        rigidBody->setAngularVelocity(btVector3(0, 0, 0));
        
        rigidBody->setGravity(btVector3(0, 0, 0));
        
        mass = 0.0f;
        btVector3 localInertiaFinal(0, 0, 0);
        rigidBody->setMassProps(mass, localInertiaFinal);
#else
        int flags = rigidBody->getCollisionFlags();
        
        flags &= ~btCollisionObject::CF_STATIC_OBJECT;
        flags &= ~btCollisionObject::CF_KINEMATIC_OBJECT;
        
        if (type == PhysicsBodyType::STATIC) {
            flags |= btCollisionObject::CF_STATIC_OBJECT;
            mass = 0.0f;
        } else if (type == PhysicsBodyType::KINEMATIC) {
            flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
            mass = 0.0f;
        } else {
            if (mass <= 0.0f) mass = 1.0f;
        }
        
        rigidBody->setCollisionFlags(flags);
        
        btVector3 localInertia(0, 0, 0);
        if (mass > 0.0f && collisionShape) {
            collisionShape->calculateLocalInertia(mass, localInertia);
        }
        rigidBody->setMassProps(mass, localInertia);
        
        if (type == PhysicsBodyType::KINEMATIC) {
            rigidBody->setGravity(btVector3(0, 0, 0));
        }
        
        rigidBody->setActivationState(ACTIVE_TAG);
#endif
    }
}

void PhysicsComponent::setMass(float newMass) {
    mass = newMass;
    
    if (rigidBody && collisionShape) {
        btVector3 localInertia(0, 0, 0);
        if (mass > 0.0f) {
            collisionShape->calculateLocalInertia(mass, localInertia);
        }
        rigidBody->setMassProps(mass, localInertia);
    }
}

void PhysicsComponent::setFriction(float newFriction) {
    friction = newFriction;
    if (rigidBody) {
        rigidBody->setFriction(friction);
    }
}

void PhysicsComponent::setRestitution(float newRestitution) {
    restitution = newRestitution;
    if (rigidBody) {
        rigidBody->setRestitution(restitution);
    }
}

void PhysicsComponent::setLinearDamping(float newDamping) {
    linearDamping = newDamping;
    if (rigidBody) {
        rigidBody->setDamping(linearDamping, angularDamping);
    }
}

void PhysicsComponent::setAngularDamping(float newDamping) {
    angularDamping = newDamping;
    if (rigidBody) {
        rigidBody->setDamping(linearDamping, angularDamping);
    }
}

void PhysicsComponent::setLinearVelocity(const glm::vec3& velocity) {
    if (rigidBody) {
        rigidBody->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    }
}

glm::vec3 PhysicsComponent::getLinearVelocity() const {
    if (rigidBody) {
        btVector3 velocity = rigidBody->getLinearVelocity();
        return glm::vec3(velocity.x(), velocity.y(), velocity.z());
    }
    return glm::vec3(0.0f);
}

void PhysicsComponent::setAngularVelocity(const glm::vec3& velocity) {
    if (rigidBody) {
        rigidBody->setAngularVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    }
}

glm::vec3 PhysicsComponent::getAngularVelocity() const {
    if (rigidBody) {
        btVector3 velocity = rigidBody->getAngularVelocity();
        return glm::vec3(velocity.x(), velocity.y(), velocity.z());
    }
    return glm::vec3(0.0f);
}

void PhysicsComponent::applyForce(const glm::vec3& force, const glm::vec3& point) {
    if (rigidBody) {
        rigidBody->applyForce(btVector3(force.x, force.y, force.z), 
                             btVector3(point.x, point.y, point.z));
    }
}

void PhysicsComponent::applyImpulse(const glm::vec3& impulse, const glm::vec3& point) {
    if (rigidBody) {
        rigidBody->applyImpulse(btVector3(impulse.x, impulse.y, impulse.z), 
                               btVector3(point.x, point.y, point.z));
    }
}

void PhysicsComponent::applyTorque(const glm::vec3& torque) {
    if (rigidBody) {
        rigidBody->applyTorque(btVector3(torque.x, torque.y, torque.z));
    }
}

void PhysicsComponent::applyTorqueImpulse(const glm::vec3& torque) {
    if (rigidBody) {
        rigidBody->applyTorqueImpulse(btVector3(torque.x, torque.y, torque.z));
    }
}

void PhysicsComponent::syncTransformFromPhysics() {
    if (rigidBody && owner) {
        btTransform transform;
        rigidBody->getMotionState()->getWorldTransform(transform);
        
        btVector3 pos = transform.getOrigin();
        btQuaternion rot = transform.getRotation();
        
#ifdef EDITOR_BUILD
        // EDITOR MODE: Simple synchronization (no bidirectional sync)
        owner->getTransform().setPosition(glm::vec3(pos.x(), pos.y(), pos.z()));
        owner->getTransform().setRotation(glm::quat(rot.w(), rot.x(), rot.y(), rot.z()));
#else
        // GAME MODE: Handle both root-level and parent-child physics objects
        glm::vec3 physicsWorldPos = glm::vec3(pos.x(), pos.y(), pos.z());
        glm::quat physicsWorldRot = glm::quat(rot.w(), rot.x(), rot.y(), rot.z());
        
        static int syncDebugCounter = 0;
        syncDebugCounter++;
        
        if (syncDebugCounter % 60 == 0) {
            glm::mat4 currentWorldTransform = owner->getWorldMatrix();
            glm::vec3 currentWorldPos = glm::vec3(currentWorldTransform[3]);
            glm::quat currentWorldRot = glm::quat_cast(currentWorldTransform);
            
            float positionDifference = glm::length(physicsWorldPos - currentWorldPos);
            float rotationDifference = glm::length(glm::vec4(physicsWorldRot.x, physicsWorldRot.y, physicsWorldRot.z, physicsWorldRot.w) - 
                                                 glm::vec4(currentWorldRot.x, currentWorldRot.y, currentWorldRot.z, currentWorldRot.w));
            
            if (positionDifference > 0.01f || rotationDifference > 0.01f) {
            }
        }
        
        // For DYNAMIC and KINEMATIC bodies, we need to handle transform synchronization
        if (bodyType == PhysicsBodyType::DYNAMIC || bodyType == PhysicsBodyType::KINEMATIC) {
            if (owner->getParent()) {
                // PARENT-CHILD HIERARCHY: 
                // For KINEMATIC bodies that are children, the PARENT controls movement (not physics)
                // So we should NOT sync from physics back to parent - parent is authoritative
                // Only sync if this is a DYNAMIC body (physics controls it)
                if (bodyType == PhysicsBodyType::DYNAMIC) {
                    // DYNAMIC body: Physics controls movement, update parent
                    auto& parentTransform = owner->getParent()->getTransform();
                    parentTransform.setPosition(physicsWorldPos);
                    parentTransform.setRotation(physicsWorldRot);
                } else {
                    // KINEMATIC body: Parent (PlayerRoot) controls movement via scripts
                    // DO NOT sync from physics to parent - parent is authoritative!
                    // The parent's position is set by Lua scripts (setNodePosition)
                    // Physics should sync TO parent's position (done in syncTransformToPhysics)
                    // We do NOTHING here - don't overwrite the parent's position
                }
            } else {
                // ROOT LEVEL: Direct synchronization (no parent-child hierarchy)
                owner->getTransform().setPosition(physicsWorldPos);
                owner->getTransform().setRotation(physicsWorldRot);
            }
        } else {
            // For STATIC bodies, DO NOT sync from physics - static bodies never move
            // The node controls physics, not the other way around
            // If physics position is wrong, sync TO physics instead (done in syncTransformToPhysics)
            // Do nothing here - static bodies maintain their position from the node
        }
#endif
    }
}

void PhysicsComponent::syncTransformToPhysics() {
    if (rigidBody && owner) {
        btTransform transform;
        transform.setIdentity();
        
#ifdef EDITOR_BUILD
        // EDITOR MODE: Simple synchronization (no complex position correction)
        glm::mat4 worldTransform = owner->getWorldMatrix();
        glm::vec3 worldPos = glm::vec3(worldTransform[3]);
        
        // Basic validation only
        if (std::isnan(worldPos.x) || std::isnan(worldPos.y) || std::isnan(worldPos.z) ||
            std::isinf(worldPos.x) || std::isinf(worldPos.y) || std::isinf(worldPos.z)) {
            printf("PhysicsComponent::syncTransformToPhysics - ERROR: Invalid world position detected!\n");
            return; // Skip update in editor if position is invalid
        }
        
        transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
        transform.setRotation(btQuaternion(0, 0, 0, 1)); // Identity rotation
        
        rigidBody->getMotionState()->setWorldTransform(transform);
#else
        // GAME MODE: Full synchronization with position correction
        // Use WORLD transform (including parent transforms)
        glm::mat4 worldTransform = owner->getWorldMatrix();
        
        // SAFER matrix extraction with validation
        glm::vec3 worldPos;
        
        // Extract position from the last column of the matrix
        worldPos = glm::vec3(worldTransform[3]);
        
        // Validate the extracted position
        if (std::isnan(worldPos.x) || std::isnan(worldPos.y) || std::isnan(worldPos.z) ||
            std::isinf(worldPos.x) || std::isinf(worldPos.y) || std::isinf(worldPos.z)) {
            
            // Fallback to local position if world transform is invalid
            auto& localTransform = owner->getTransform();
            worldPos = localTransform.getPosition();
        }
        
        const float MAX_POSITION = 1000000.0f;
        if (std::abs(worldPos.x) > MAX_POSITION || std::abs(worldPos.y) > MAX_POSITION || std::abs(worldPos.z) > MAX_POSITION) {
            
            worldPos.x = glm::clamp(worldPos.x, -MAX_POSITION, MAX_POSITION);
            worldPos.y = glm::clamp(worldPos.y, -MAX_POSITION, MAX_POSITION);
            worldPos.z = glm::clamp(worldPos.z, -MAX_POSITION, MAX_POSITION);
        }
        
        if (owner->getParent()) {
            auto& parentTransform = owner->getParent()->getTransform();
            glm::vec3 parentPos = parentTransform.getPosition();
            auto& localTransform = owner->getTransform();
            glm::vec3 localPos = localTransform.getPosition();
            
            glm::vec3 expectedWorldPos = parentPos + localPos;
            
            float positionDifference = glm::length(worldPos - expectedWorldPos);
            const float MAX_POSITION_DIFFERENCE = 50.0f;
            
            if (positionDifference > MAX_POSITION_DIFFERENCE) {
                
                worldPos = expectedWorldPos;
            }
        }
        
        transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
        
        // Extract and use the actual world rotation from the transform matrix
        glm::quat worldRot = glm::quat_cast(worldTransform);
        transform.setRotation(btQuaternion(worldRot.x, worldRot.y, worldRot.z, worldRot.w));
        
        rigidBody->getMotionState()->setWorldTransform(transform);
        
        if (bodyType == PhysicsBodyType::KINEMATIC) {
            rigidBody->setWorldTransform(transform);
            rigidBody->setActivationState(ACTIVE_TAG);

            btVector3 aabbMin, aabbMax;
            if (collisionShape) {
                collisionShape->getAabb(transform, aabbMin, aabbMax);
                btBroadphaseInterface* broadphase = PhysicsManager::getInstance().getDynamicsWorld()->getBroadphase();
                if (broadphase) {
                    broadphase->setAabb(rigidBody->getBroadphaseHandle(), aabbMin, aabbMax, 
                                      PhysicsManager::getInstance().getDynamicsWorld()->getDispatcher());
                }
            }
        }
#endif
    }
}

bool PhysicsComponent::isColliding() const {
    return isCollidingFlag;
}

void PhysicsComponent::setCollisionCallback(std::function<void(PhysicsComponent*)> callback) {
    collisionCallback = callback;
}

void PhysicsComponent::setShowCollisionShape(bool show) {
    showCollisionShape = show;
}

void PhysicsComponent::createRigidBody() {
    if (!owner) return;
    
    collisionShape = createBulletCollisionShape();
    if (!collisionShape) return;
    
    glm::mat4 worldTransform = owner->getWorldMatrix();
    
    glm::vec3 worldPos;
    glm::quat worldRot;
    
    worldPos = glm::vec3(worldTransform[3]);
    
    if (std::isnan(worldPos.x) || std::isnan(worldPos.y) || std::isnan(worldPos.z) ||
        std::isinf(worldPos.x) || std::isinf(worldPos.y) || std::isinf(worldPos.z)) {
        
        printf("PhysicsComponent::createRigidBody - ERROR: Invalid world position detected!\n");
        printf("  Raw matrix values: [3][0]=%.6f, [3][1]=%.6f, [3][2]=%.6f\n", 
               worldTransform[3][0], worldTransform[3][1], worldTransform[3][2]);
        
        auto& localTransform = owner->getTransform();
        worldPos = localTransform.getPosition();
        printf("  Falling back to local position: (%.3f, %.3f, %.3f)\n", worldPos.x, worldPos.y, worldPos.z);
    }
    
    const float MAX_POSITION = 1000000.0f;
    if (std::abs(worldPos.x) > MAX_POSITION || std::abs(worldPos.y) > MAX_POSITION || std::abs(worldPos.z) > MAX_POSITION) {
        printf("PhysicsComponent::createRigidBody - WARNING: Extremely large position detected!\n");
        printf("  Position: (%.3f, %.3f, %.3f)\n", worldPos.x, worldPos.y, worldPos.z);
        
        worldPos.x = glm::clamp(worldPos.x, -MAX_POSITION, MAX_POSITION);
        worldPos.y = glm::clamp(worldPos.y, -MAX_POSITION, MAX_POSITION);
        worldPos.z = glm::clamp(worldPos.z, -MAX_POSITION, MAX_POSITION);
        printf("  Clamped to: (%.3f, %.3f, %.3f)\n", worldPos.x, worldPos.y, worldPos.z);
    }
    
    worldRot = glm::quat_cast(worldTransform);
    
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
    transform.setRotation(btQuaternion(worldRot.x, worldRot.y, worldRot.z, worldRot.w));
    
    motionState = new btDefaultMotionState(transform);
    
    btVector3 localInertia(0, 0, 0);
    if (mass > 0.0f) {
        collisionShape->calculateLocalInertia(mass, localInertia);
    }
    
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, collisionShape, localInertia);
    rigidBody = new btRigidBody(rbInfo);
    
    rigidBody->setUserPointer(this);
    
    rigidBody->setFriction(friction);
    rigidBody->setRestitution(restitution);
    rigidBody->setDamping(linearDamping, angularDamping);
    
    rigidBody->setContactProcessingThreshold(0.0f);
    
#ifdef EDITOR_BUILD
    int flags = btCollisionObject::CF_STATIC_OBJECT;
    rigidBody->setCollisionFlags(flags);
    rigidBody->setActivationState(DISABLE_SIMULATION);
    
    rigidBody->setLinearVelocity(btVector3(0, 0, 0));
    rigidBody->setAngularVelocity(btVector3(0, 0, 0));
    
    rigidBody->setGravity(btVector3(0, 0, 0));
    
    mass = 0.0f;
    btVector3 localInertiaFinal(0, 0, 0);
    rigidBody->setMassProps(mass, localInertiaFinal);
#else
    int flags = 0;
    if (bodyType == PhysicsBodyType::STATIC) {
        flags |= btCollisionObject::CF_STATIC_OBJECT;
        mass = 0.0f;
    } else if (bodyType == PhysicsBodyType::KINEMATIC) {
        flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
        mass = 0.0f;
    } else {
        if (mass <= 0.0f) mass = 1.0f;
        
        rigidBody->setCollisionFlags(btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
        
        rigidBody->setCcdMotionThreshold(0.1f);
        rigidBody->setCcdSweptSphereRadius(0.1f);
        
        rigidBody->setAngularFactor(btVector3(1.0f, 1.0f, 1.0f));
        rigidBody->setLinearFactor(btVector3(1.0f, 1.0f, 1.0f));
        
        rigidBody->setSleepingThresholds(0.1f, 0.1f);
    }
    
    rigidBody->setCollisionFlags(flags);
    
    if (bodyType == PhysicsBodyType::KINEMATIC) {
        rigidBody->setGravity(btVector3(0, 0, 0));
    }
    
    btVector3 localInertiaFinal(0, 0, 0);
    if (mass > 0.0f && collisionShape) {
        collisionShape->calculateLocalInertia(mass, localInertiaFinal);
    }
    rigidBody->setMassProps(mass, localInertiaFinal);
    
    rigidBody->setActivationState(ACTIVE_TAG);
#endif
    
    PhysicsManager::getInstance().addRigidBody(rigidBody);
}

void PhysicsComponent::destroyRigidBody() {
    if (rigidBody) {
        PhysicsManager::getInstance().removeRigidBody(rigidBody);
        delete rigidBody;
        rigidBody = nullptr;
    }
    
    if (motionState) {
        delete motionState;
        motionState = nullptr;
    }
    
    if (collisionShape) {
        delete collisionShape;
        collisionShape = nullptr;
    }
}

void PhysicsComponent::updateCollisionShape() {
    if (rigidBody) {
        PhysicsManager::getInstance().removeRigidBody(rigidBody);
        
        if (collisionShape) {
            delete collisionShape;
        }
        
        collisionShape = createBulletCollisionShape();
        
        rigidBody->setCollisionShape(collisionShape);
        
        btVector3 localInertia(0, 0, 0);
        if (mass > 0.0f && collisionShape) {
            collisionShape->calculateLocalInertia(mass, localInertia);
        }
        rigidBody->setMassProps(mass, localInertia);
        
        if (owner) {
            glm::mat4 worldTransform = owner->getWorldMatrix();
            
            glm::vec3 worldPos;
            glm::quat worldRot;
            
            worldPos = glm::vec3(worldTransform[3]);
            
            if (std::isnan(worldPos.x) || std::isnan(worldPos.y) || std::isnan(worldPos.z) ||
                std::isinf(worldPos.x) || std::isinf(worldPos.y) || std::isinf(worldPos.z)) {
                
                printf("PhysicsComponent::updateCollisionShape - ERROR: Invalid world position detected!\n");
                printf("  Raw matrix values: [3][0]=%.6f, [3][1]=%.6f, [3][2]=%.6f\n", 
                       worldTransform[3][0], worldTransform[3][1], worldTransform[3][2]);
                
                auto& localTransform = owner->getTransform();
                worldPos = localTransform.getPosition();
                printf("  Falling back to local position: (%.3f, %.3f, %.3f)\n", worldPos.x, worldPos.y, worldPos.z);
            }
            
            const float MAX_POSITION = 1000000.0f;
            if (std::abs(worldPos.x) > MAX_POSITION || std::abs(worldPos.y) > MAX_POSITION || std::abs(worldPos.z) > MAX_POSITION) {
                printf("PhysicsComponent::updateCollisionShape - WARNING: Extremely large position detected!\n");
                printf("  Position: (%.3f, %.3f, %.3f)\n", worldPos.x, worldPos.y, worldPos.z);
                
                worldPos.x = glm::clamp(worldPos.x, -MAX_POSITION, MAX_POSITION);
                worldPos.y = glm::clamp(worldPos.y, -MAX_POSITION, MAX_POSITION);
                worldPos.z = glm::clamp(worldPos.z, -MAX_POSITION, MAX_POSITION);
                printf("  Clamped to: (%.3f, %.3f, %.3f)\n", worldPos.x, worldPos.y, worldPos.z);
            }
            
            worldRot = glm::quat_cast(worldTransform);
            
            btTransform transform;
            transform.setIdentity();
            transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
            transform.setRotation(btQuaternion(worldRot.x, worldRot.y, worldRot.z, worldRot.w));
            
            rigidBody->getMotionState()->setWorldTransform(transform);
        }
        
        PhysicsManager::getInstance().addRigidBody(rigidBody);
    }
}

btCollisionShape* PhysicsComponent::createBulletCollisionShape() {
    glm::mat4 worldTransform = glm::mat4(1.0f);
    if (owner) {
        worldTransform = owner->getWorldMatrix();
    }
    
    glm::vec3 worldPos = glm::vec3(worldTransform[3]);
    glm::quat worldRot = glm::quat_cast(worldTransform);
    
    glm::vec3 worldScale;
    worldScale.x = glm::length(glm::vec3(worldTransform[0]));
    worldScale.y = glm::length(glm::vec3(worldTransform[1]));
    worldScale.z = glm::length(glm::vec3(worldTransform[2]));
    
    glm::vec3 scaledDimensions = shapeDimensions * worldScale;
    
    switch (collisionShapeType) {
        case CollisionShapeType::BOX:
            return PhysicsManager::getInstance().createBoxShape(scaledDimensions * 0.5f);
            
        case CollisionShapeType::SPHERE: {
            float maxScale = std::max({worldScale.x, worldScale.y, worldScale.z});
            return PhysicsManager::getInstance().createSphereShape(shapeDimensions.x * 0.5f * maxScale);
        }
            
        case CollisionShapeType::CAPSULE: {
            float radiusScale = std::max(worldScale.x, worldScale.z);
            return PhysicsManager::getInstance().createCapsuleShape(
                shapeDimensions.x * 0.5f * radiusScale, 
                shapeDimensions.y * worldScale.y);
        }
            
        case CollisionShapeType::CYLINDER:
            return PhysicsManager::getInstance().createCylinderShape(scaledDimensions * 0.5f);
            
        case CollisionShapeType::PLANE: {
            glm::vec3 rotatedNormal = glm::normalize(worldRot * glm::vec3(0, 1, 0));
            
            float planeConstant = -(rotatedNormal.x * worldPos.x + 
                                   rotatedNormal.y * worldPos.y + 
                                   rotatedNormal.z * worldPos.z);
            
            btCollisionShape* planeShape = PhysicsManager::getInstance().createPlaneShape(rotatedNormal, planeConstant);
            
            if (std::abs(worldScale.x - 1.0f) > 0.01f || std::abs(worldScale.z - 1.0f) > 0.01f) {
                printf("PhysicsComponent::createBulletCollisionShape - WARNING: Plane has non-uniform scaling, using large box as fallback\n");
                printf("  Original plane normal: (0, 1, 0), Rotated normal: (%.3f, %.3f, %.3f)\n", 
                       rotatedNormal.x, rotatedNormal.y, rotatedNormal.z);
                printf("  World scale: (%.3f, %.3f, %.3f)\n", worldScale.x, worldScale.y, worldScale.z);
                
                delete planeShape;
                
                glm::vec3 boxDimensions = glm::vec3(
                    shapeDimensions.x * worldScale.x * 10.0f,
                    shapeDimensions.y * worldScale.y * 0.1f,
                    shapeDimensions.z * worldScale.z * 10.0f
                );
                return PhysicsManager::getInstance().createBoxShape(boxDimensions * 0.5f);
            }
            
            return planeShape;
        }
            
        default:
            return PhysicsManager::getInstance().createBoxShape(glm::vec3(0.5f));
    }
}

glm::vec3 PhysicsComponent::bulletToGlm(const void* bulletVec) const {
    const btVector3* vec = static_cast<const btVector3*>(bulletVec);
    return glm::vec3(vec->x(), vec->y(), vec->z());
}

void PhysicsComponent::glmToBullet(const glm::vec3& glmVec, void* bulletVec) const {
    btVector3* vec = static_cast<btVector3*>(bulletVec);
    *vec = btVector3(glmVec.x, glmVec.y, glmVec.z);
}

void PhysicsComponent::render(Renderer& renderer) {
    // Only render collision shapes if enabled
    if (!showCollisionShape || !collisionShape || !owner) {
        return;
    }
}

void PhysicsComponent::drawCollisionShape() const {
    if (!showCollisionShape || !collisionShape || !owner) {
        return;
    }
}

void PhysicsComponent::renderDebugShape(Material& debugMaterial,
                                       const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!showCollisionShape || !owner) return;
    
    std::shared_ptr<Mesh> wireframeMesh;
    
    switch (collisionShapeType) {
        case CollisionShapeType::BOX:
            wireframeMesh = Mesh::createWireframeBox(shapeDimensions * 0.5f);
            break;
            
        case CollisionShapeType::SPHERE:
            wireframeMesh = Mesh::createWireframeSphere(shapeDimensions.x * 0.5f, 16);
            break;
            
        case CollisionShapeType::CAPSULE:
            wireframeMesh = Mesh::createWireframeCapsule(shapeDimensions.x * 0.5f, shapeDimensions.y, 16);
            break;
            
        case CollisionShapeType::CYLINDER:
            wireframeMesh = Mesh::createWireframeCylinder(shapeDimensions.x * 0.5f, shapeDimensions.y, 16);
            break;
            
        case CollisionShapeType::PLANE:
            wireframeMesh = Mesh::createWireframePlane(shapeDimensions.x, shapeDimensions.z);
            break;
            
        default:
            return; // Unknown shape type
    }
    
    if (wireframeMesh) {
        glm::mat4 worldTransform = owner->getWorldMatrix();
        
        wireframeMesh->bind();
        
        debugMaterial.apply();
        
        auto shader = debugMaterial.getShader();
        if (shader) {
            shader->setMat4("modelMatrix", worldTransform);
            shader->setMat4("viewMatrix", viewMatrix);
            shader->setMat4("projectionMatrix", projectionMatrix);
        }
        
        wireframeMesh->draw();
        
        wireframeMesh->unbind();
    }
}

void PhysicsComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::TreeNode("Physics Component")) {
        const char* shapeTypes[] = { "Box", "Sphere", "Capsule", "Cylinder", "Plane" };
        int currentShape = static_cast<int>(collisionShapeType);
        if (ImGui::Combo("Collision Shape", &currentShape, shapeTypes, 5)) {
            setCollisionShape(static_cast<CollisionShapeType>(currentShape));
        }
        
        glm::vec3 dims = shapeDimensions;
        if (ImGui::DragFloat3("Dimensions", &dims.x, 0.1f)) {
            setCollisionShape(collisionShapeType, dims);
        }
        
        const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
        int currentBody = static_cast<int>(bodyType);
        if (ImGui::Combo("Body Type", &currentBody, bodyTypes, 3)) {
            setBodyType(static_cast<PhysicsBodyType>(currentBody));
        }
        
        float massValue = mass;
        if (ImGui::DragFloat("Mass", &massValue, 0.1f, 0.0f, 1000.0f)) {
            setMass(massValue);
        }
        
        // Friction
        float frictionValue = friction;
        if (ImGui::DragFloat("Friction", &frictionValue, 0.01f, 0.0f, 2.0f)) {
            setFriction(frictionValue);
        }
        
        // Restitution
        float restitutionValue = restitution;
        if (ImGui::DragFloat("Restitution", &restitutionValue, 0.01f, 0.0f, 1.0f)) {
            setRestitution(restitutionValue);
        }
        
        float linearDampingValue = linearDamping;
        if (ImGui::DragFloat("Linear Damping", &linearDampingValue, 0.01f, 0.0f, 1.0f)) {
            setLinearDamping(linearDampingValue);
        }
        
        float angularDampingValue = angularDamping;
        if (ImGui::DragFloat("Angular Damping", &angularDampingValue, 0.01f, 0.0f, 1.0f)) {
            setAngularDamping(angularDampingValue);
        }
        
        bool showShape = showCollisionShape;
        if (ImGui::Checkbox("Show Collision Shape", &showShape)) {
            setShowCollisionShape(showShape);
        }
        
        // Debug information
        if (ImGui::CollapsingHeader("Debug Info")) {
            if (owner) {
                auto& transform = owner->getTransform();
                glm::vec3 localPos = transform.getPosition();
                glm::mat4 worldTransform = owner->getWorldMatrix();
                glm::vec3 worldPos = glm::vec3(worldTransform[3]);
                
                ImGui::Text("Local Position: (%.3f, %.3f, %.3f)", localPos.x, localPos.y, localPos.z);
                ImGui::Text("World Position: (%.3f, %.3f, %.3f)", worldPos.x, worldPos.y, worldPos.z);
                
                if (rigidBody) {
                    btTransform bulletTransform = rigidBody->getWorldTransform();
                    btVector3 bulletPos = bulletTransform.getOrigin();
                    ImGui::Text("Bullet Position: (%.3f, %.3f, %.3f)", bulletPos.x(), bulletPos.y(), bulletPos.z());
                }
            }
            
            if (ImGui::Button("Force Update Collision Shape")) {
                forceUpdateCollisionShape();
            }
        }
        
        ImGui::TreePop();
    }
#endif
}

} // namespace GameEngine
