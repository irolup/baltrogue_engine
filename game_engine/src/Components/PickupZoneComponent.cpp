#include "Components/PickupZoneComponent.h"
#include "Components/PhysicsComponent.h"
#include "Physics/PhysicsManager.h"
#include "Scene/SceneNode.h"
#include "Core/Transform.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include <algorithm>
#include <iostream>

// Bullet includes
#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

// ImGui for editor
#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

PickupZoneComponent::PickupZoneComponent()
    : shapeType(PickupZoneShape::BOX)
    , dimensions(1.0f, 1.0f, 1.0f)
    , radius(0.5f)
    , height(1.0f)
    , detectionRadius(1.0f)
    , continuousDetection(true)
    , ghostObject(nullptr)
    , collisionShape(nullptr)
    , showDebugShape(true)
{
}

PickupZoneComponent::~PickupZoneComponent() {
    destroy();
}

void PickupZoneComponent::start() {
    // Create collision shape and ghost object
    createCollisionShape();
    createGhostObject();
    
    // Initialize state tracking
    objectsInZone.clear();
    previousObjectsInZone.clear();
}

void PickupZoneComponent::update(float deltaTime) {
    if (!ghostObject || !owner) {
        return;
    }
    
    // Update the ghost object's transform to match the node's world transform
    glm::vec3 worldPos = getWorldPosition();
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
    
    // Update the ghost object's world transform
    ghostObject->setWorldTransform(transform);
    
    // Perform collision detection
    performCollisionDetection();
    
    // Handle collision events
    handleCollisionEvents();
}

void PickupZoneComponent::destroy() {
    destroyGhostObject();
}

void PickupZoneComponent::setShape(PickupZoneShape shape) {
    shapeType = shape;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void PickupZoneComponent::setDimensions(const glm::vec3& dims) {
    dimensions = dims;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void PickupZoneComponent::setRadius(float newRadius) {
    radius = newRadius;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void PickupZoneComponent::setHeight(float newHeight) {
    height = newHeight;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void PickupZoneComponent::setDetectionTags(const std::vector<std::string>& tags) {
    detectionTags = tags;
}

void PickupZoneComponent::setDetectionRadius(float newRadius) {
    detectionRadius = newRadius;
}

void PickupZoneComponent::setContinuousDetection(bool enabled) {
    continuousDetection = enabled;
}

void PickupZoneComponent::setOnEnterCallback(std::function<void(const std::string&)> callback) {
    onEnterCallback = callback;
}

void PickupZoneComponent::setOnExitCallback(std::function<void(const std::string&)> callback) {
    onExitCallback = callback;
}

void PickupZoneComponent::setOnStayCallback(std::function<void(const std::string&)> callback) {
    onStayCallback = callback;
}

bool PickupZoneComponent::isObjectInZone(const std::string& objectTag) const {
    return std::find(objectsInZone.begin(), objectsInZone.end(), objectTag) != objectsInZone.end();
}

std::vector<std::string> PickupZoneComponent::getObjectsInZone() const {
    return objectsInZone;
}

size_t PickupZoneComponent::getObjectCount() const {
    return objectsInZone.size();
}

void PickupZoneComponent::setShowDebugShape(bool show) {
    showDebugShape = show;
}

bool PickupZoneComponent::getShowDebugShape() const {
    return showDebugShape;
}

void PickupZoneComponent::createCollisionShape() {
    collisionShape = createBulletCollisionShape();
}

void PickupZoneComponent::createGhostObject() {
    if (!collisionShape) {
        return;
    }
    
    // Create ghost object for trigger detection
    // Note: We'll use a regular btCollisionObject with special flags
    ghostObject = new btCollisionObject();
    ghostObject->setCollisionShape(collisionShape);
    ghostObject->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    
    // Set initial transform
    glm::vec3 worldPos = getWorldPosition();
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
    ghostObject->setWorldTransform(transform);
    
    // Add to physics world for collision detection
    PhysicsManager::getInstance().getDynamicsWorld()->addCollisionObject(ghostObject);
}

void PickupZoneComponent::destroyGhostObject() {
    if (ghostObject) {
        PhysicsManager::getInstance().getDynamicsWorld()->removeCollisionObject(ghostObject);
        delete ghostObject;
        ghostObject = nullptr;
    }
    
    if (collisionShape) {
        delete collisionShape;
        collisionShape = nullptr;
    }
}

void PickupZoneComponent::updateCollisionShape() {
    if (!ghostObject) {
        return;
    }
    
    // Remove from world temporarily
    PhysicsManager::getInstance().getDynamicsWorld()->removeCollisionObject(ghostObject);
    
    // Delete old collision shape
    if (collisionShape) {
        delete collisionShape;
    }
    
    // Create new collision shape
    collisionShape = createBulletCollisionShape();
    ghostObject->setCollisionShape(collisionShape);
    
    // Add back to world
    PhysicsManager::getInstance().getDynamicsWorld()->addCollisionObject(ghostObject);
}

void PickupZoneComponent::performCollisionDetection() {
    if (!ghostObject || !continuousDetection) {
        return;
    }
    
    // Get the dynamics world
    btDiscreteDynamicsWorld* world = PhysicsManager::getInstance().getDynamicsWorld();
    if (!world) {
        return;
    }
    
    // Clear current objects in zone
    previousObjectsInZone = objectsInZone;
    objectsInZone.clear();
    
    // Simple distance-based collision detection for now
    // This is a simplified approach that works without btGhostObject
    glm::vec3 zonePos = getWorldPosition();
    
    // Check all collision objects in the world
    for (int i = 0; i < world->getNumCollisionObjects(); i++) {
        btCollisionObject* obj = world->getCollisionObjectArray()[i];
        
        // Skip if this is our own ghost object
        if (obj == ghostObject) {
            continue;
        }
        
        // Get the object's world position
        btTransform objTransform = obj->getWorldTransform();
        btVector3 objPos = objTransform.getOrigin();
        glm::vec3 objectPos = glm::vec3(objPos.x(), objPos.y(), objPos.z());
        
        // Calculate distance between zone and object
        float distance = glm::length(zonePos - objectPos);
        
        // Check if object is within detection radius
        if (distance <= detectionRadius) {
            // Try to get the PhysicsComponent from the user pointer
            PhysicsComponent* physicsComp = static_cast<PhysicsComponent*>(obj->getUserPointer());
            if (physicsComp && physicsComp->getOwner()) {
                // Get the node name as the object identifier
                std::string objectTag = physicsComp->getOwner()->getName();
                
                // Debug output
                std::cout << "PICKUP ZONE: Checking object '" << objectTag << "' at distance " << distance << " (radius: " << detectionRadius << ")" << std::endl;
                
                // Check if this object matches our detection tags
                if (detectionTags.empty() || 
                    std::find(detectionTags.begin(), detectionTags.end(), objectTag) != detectionTags.end()) {
                    objectsInZone.push_back(objectTag);
                    std::cout << "PICKUP ZONE: Object '" << objectTag << "' added to zone!" << std::endl;
                }
            }
        }
    }
}

void PickupZoneComponent::handleCollisionEvents() {
    // Handle enter events
    for (const auto& objectTag : objectsInZone) {
        if (std::find(previousObjectsInZone.begin(), previousObjectsInZone.end(), objectTag) == previousObjectsInZone.end()) {
            // Object entered the zone
            std::cout << "PICKUP ZONE: Object '" << objectTag << "' ENTERED pickup zone!" << std::endl;
            if (onEnterCallback) {
                onEnterCallback(objectTag);
            }
        }
    }
    
    // Handle exit events
    for (const auto& objectTag : previousObjectsInZone) {
        if (std::find(objectsInZone.begin(), objectsInZone.end(), objectTag) == objectsInZone.end()) {
            // Object exited the zone
            std::cout << "PICKUP ZONE: Object '" << objectTag << "' EXITED pickup zone!" << std::endl;
            if (onExitCallback) {
                onExitCallback(objectTag);
            }
        }
    }
    
    // Handle stay events
    for (const auto& objectTag : objectsInZone) {
        if (onStayCallback) {
            onStayCallback(objectTag);
        }
    }
}

glm::vec3 PickupZoneComponent::getWorldPosition() const {
    if (!owner) {
        return glm::vec3(0.0f);
    }
    
    glm::mat4 worldTransform = owner->getWorldMatrix();
    return glm::vec3(worldTransform[3]);
}

btCollisionShape* PickupZoneComponent::createBulletCollisionShape() {
    switch (shapeType) {
        case PickupZoneShape::BOX:
            return PhysicsManager::getInstance().createBoxShape(dimensions * 0.5f);
            
        case PickupZoneShape::SPHERE:
            return PhysicsManager::getInstance().createSphereShape(radius);
            
        case PickupZoneShape::CAPSULE:
            return PhysicsManager::getInstance().createCapsuleShape(radius, height);
            
        default:
            return PhysicsManager::getInstance().createBoxShape(glm::vec3(0.5f));
    }
}

void PickupZoneComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::TreeNode("Pickup Zone Component")) {
        // Shape type
        const char* shapeTypes[] = { "Box", "Sphere", "Capsule" };
        int currentShape = static_cast<int>(shapeType);
        if (ImGui::Combo("Shape Type", &currentShape, shapeTypes, 3)) {
            setShape(static_cast<PickupZoneShape>(currentShape));
        }
        
        // Dimensions (for box)
        if (shapeType == PickupZoneShape::BOX) {
            glm::vec3 dims = dimensions;
            if (ImGui::DragFloat3("Dimensions", &dims.x, 0.1f)) {
                setDimensions(dims);
            }
        }
        
        // Radius (for sphere and capsule)
        if (shapeType == PickupZoneShape::SPHERE || shapeType == PickupZoneShape::CAPSULE) {
            float rad = radius;
            if (ImGui::DragFloat("Radius", &rad, 0.1f)) {
                setRadius(rad);
            }
        }
        
        // Height (for capsule)
        if (shapeType == PickupZoneShape::CAPSULE) {
            float h = height;
            if (ImGui::DragFloat("Height", &h, 0.1f)) {
                setHeight(h);
            }
        }
        
        // Detection settings
        ImGui::Separator();
        ImGui::Text("Detection Settings");
        
        float detRadius = detectionRadius;
        if (ImGui::DragFloat("Detection Radius", &detRadius, 0.1f)) {
            setDetectionRadius(detRadius);
        }
        
        bool continuous = continuousDetection;
        if (ImGui::Checkbox("Continuous Detection", &continuous)) {
            setContinuousDetection(continuous);
        }
        
        // Debug visualization
        ImGui::Separator();
        bool showDebug = showDebugShape;
        if (ImGui::Checkbox("Show Debug Shape", &showDebug)) {
            setShowDebugShape(showDebug);
        }
        
        // Current state
        ImGui::Separator();
        ImGui::Text("Current State");
        ImGui::Text("Objects in zone: %zu", getObjectCount());
        
        if (ImGui::CollapsingHeader("Objects in Zone")) {
            for (const auto& objectTag : objectsInZone) {
                ImGui::BulletText("%s", objectTag.c_str());
            }
        }
        
        ImGui::TreePop();
    }
#endif
}

} // namespace GameEngine
