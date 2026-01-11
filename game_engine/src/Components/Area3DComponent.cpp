#include "Components/Area3DComponent.h"
#include "Physics/PhysicsManager.h"
#include "Scene/SceneNode.h"
#include "Core/Transform.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Components/PhysicsComponent.h"
#include <algorithm>
#include <iostream>

// OpenGL for wireframe rendering
#ifdef LINUX_BUILD
#include <GL/gl.h>
#endif

// Bullet includes
#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

// ImGui for editor
#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

// Static group registry initialization
std::unordered_map<std::string, std::vector<Area3DComponent*>> Area3DComponent::groupRegistry;

// Helper function to convert Area3DShape to CollisionShapeType
CollisionShapeType areaShapeToCollisionShape(Area3DShape shape) {
    switch (shape) {
        case Area3DShape::BOX: return CollisionShapeType::BOX;
        case Area3DShape::SPHERE: return CollisionShapeType::SPHERE;
        case Area3DShape::CAPSULE: return CollisionShapeType::CAPSULE;
        case Area3DShape::CYLINDER: return CollisionShapeType::CYLINDER;
        case Area3DShape::PLANE: return CollisionShapeType::PLANE;
        default: return CollisionShapeType::BOX;
    }
}

Area3DComponent::Area3DComponent()
    : shapeType(Area3DShape::BOX)
    , dimensions(1.0f, 1.0f, 1.0f)
    , radius(0.5f)
    , height(1.0f)
    , group("")
    , monitorEnabled(true)
    , ghostObject(nullptr)
    , collisionShape(nullptr)
    , showDebugShape(true)
{
}

Area3DComponent::~Area3DComponent() {
    destroy();
}

void Area3DComponent::start() {
    // Create collision shape and ghost object
    createCollisionShape();
    createGhostObject();
    
    // Register with group if specified
    registerWithGroup();
    
    // Initialize state tracking
    bodiesInArea.clear();
    previousBodiesInArea.clear();
}

void Area3DComponent::update(float deltaTime) {
    if (!ghostObject || !owner || !monitorEnabled) {
        return;
    }
    
    // Update the ghost object's transform to match the node's world transform
    // Force recalculation of world matrix each frame to ensure it's up-to-date
    glm::mat4 worldMatrix = getWorldTransformMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    glm::quat worldRot = glm::quat_cast(worldMatrix);
    
    // Debug: Print if position seems wrong (very large Y value)
    #ifdef _DEBUG
    if (worldPos.y > 100.0f || worldPos.y < -100.0f) {
        std::cout << "Area3DComponent::update: WARNING - Large Y position detected: " << worldPos.y 
                  << " for node: " << (owner ? owner->getName() : "null") << std::endl;
    }
    #endif
    
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
    transform.setRotation(btQuaternion(worldRot.x, worldRot.y, worldRot.z, worldRot.w));
    
    // Update the ghost object's world transform
    ghostObject->setWorldTransform(transform);
    
    // Perform collision detection
    performCollisionDetection();
    
    // Handle collision events
    handleCollisionEvents();
}

void Area3DComponent::render(Renderer& renderer) {
    // Area3D components are trigger zones - they don't need regular rendering
    // Debug wireframe rendering is handled by EditorSystem::renderNodeDirectly()
}

void Area3DComponent::destroy() {
    unregisterFromGroup();
    destroyGhostObject();
}

void Area3DComponent::setShape(Area3DShape shape) {
    shapeType = shape;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void Area3DComponent::setDimensions(const glm::vec3& dims) {
    dimensions = dims;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void Area3DComponent::setRadius(float newRadius) {
    radius = newRadius;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void Area3DComponent::setHeight(float newHeight) {
    height = newHeight;
    if (ghostObject) {
        updateCollisionShape();
    }
}

void Area3DComponent::setGroup(const std::string& groupName) {
    // Unregister from old group
    unregisterFromGroup();
    
    // Set new group
    group = groupName;
    
    // Register with new group
    registerWithGroup();
}

void Area3DComponent::setDetectionTags(const std::vector<std::string>& tags) {
    detectionTags = tags;
}

bool Area3DComponent::isBodyInArea(const std::string& bodyName) const {
    return bodiesInArea.find(bodyName) != bodiesInArea.end();
}

std::vector<std::string> Area3DComponent::getBodiesInArea() const {
    return std::vector<std::string>(bodiesInArea.begin(), bodiesInArea.end());
}

size_t Area3DComponent::getBodyCount() const {
    return bodiesInArea.size();
}

std::vector<Area3DComponent*> Area3DComponent::getComponentsInGroup(const std::string& groupName) {
    auto it = groupRegistry.find(groupName);
    if (it != groupRegistry.end()) {
        return it->second;
    }
    return std::vector<Area3DComponent*>();
}

void Area3DComponent::setShowDebugShape(bool show) {
    showDebugShape = show;
}

void Area3DComponent::createCollisionShape() {
    collisionShape = createBulletCollisionShape();
}

void Area3DComponent::createGhostObject() {
    if (!collisionShape) {
        return;
    }
    
    // Create ghost object for trigger detection
    ghostObject = new btCollisionObject();
    ghostObject->setCollisionShape(collisionShape);
    ghostObject->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE | btCollisionObject::CF_STATIC_OBJECT);
    
    // Set initial transform
    glm::mat4 worldMatrix = getWorldTransformMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    glm::quat worldRot = glm::quat_cast(worldMatrix);
    
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
    transform.setRotation(btQuaternion(worldRot.x, worldRot.y, worldRot.z, worldRot.w));
    ghostObject->setWorldTransform(transform);
    
    // Store pointer to this component in user data
    ghostObject->setUserPointer(this);
    
    // Add to physics world for collision detection
    PhysicsManager::getInstance().getDynamicsWorld()->addCollisionObject(ghostObject);
}

void Area3DComponent::destroyGhostObject() {
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

void Area3DComponent::updateCollisionShape() {
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

void Area3DComponent::performCollisionDetection() {
    if (!ghostObject || !owner) {
        return;
    }
    
    // Get the dynamics world
    btDiscreteDynamicsWorld* world = PhysicsManager::getInstance().getDynamicsWorld();
    if (!world) {
        return;
    }
    
    // Save previous state
    previousBodiesInArea = bodiesInArea;
    bodiesInArea.clear();
    
    // Get area world position
    glm::vec3 areaPos = getWorldPosition();
    
    // Check all collision objects in the world
    for (int i = 0; i < world->getNumCollisionObjects(); i++) {
        btCollisionObject* obj = world->getCollisionObjectArray()[i];
        
        // Skip if this is our own ghost object (prevents self-detection)
        if (obj == ghostObject) {
            continue;
        }
        
        // Additional check: if the user pointer points to this Area3DComponent, skip it
        // We need to check this safely since userPointer might be a PhysicsComponent
        void* userPtr = obj->getUserPointer();
        if (userPtr) {
            // Try casting to Area3DComponent - only compare if cast is valid
            // (Note: This is safe even if it's actually a PhysicsComponent - the comparison will be false)
            Area3DComponent* otherAreaComp = static_cast<Area3DComponent*>(userPtr);
            // Only skip if it's actually our own component
            // We can verify this by checking if the owner matches
            if (otherAreaComp == this) {
                continue; // Skip self
            }
        }
        
        // Get the object's world position
        btTransform objTransform = obj->getWorldTransform();
        btVector3 objPos = objTransform.getOrigin();
        glm::vec3 objectPos = glm::vec3(objPos.x(), objPos.y(), objPos.z());
        
        // Get collision shape for intersection test
        btCollisionShape* objShape = obj->getCollisionShape();
        if (!objShape) {
            continue;
        }
        
        // Check if object is within detection area based on shape
        bool isInside = false;
        
        if (shapeType == Area3DShape::BOX) {
            // Box vs point/sphere check
            glm::vec3 localPos = objectPos - areaPos;
            glm::vec3 halfExtents = dimensions * 0.5f;
            isInside = (localPos.x >= -halfExtents.x && localPos.x <= halfExtents.x &&
                        localPos.y >= -halfExtents.y && localPos.y <= halfExtents.y &&
                        localPos.z >= -halfExtents.z && localPos.z <= halfExtents.z);
        } else if (shapeType == Area3DShape::SPHERE) {
            // Sphere vs point/sphere check
            float distance = glm::length(objectPos - areaPos);
            
            // If detecting another Area3DComponent with sphere shape, use sphere-sphere overlap
            // Check if the other object is also a sphere Area3DComponent
            Area3DComponent* otherAreaComp = static_cast<Area3DComponent*>(obj->getUserPointer());
            if (otherAreaComp && otherAreaComp != this && otherAreaComp->getOwner() && 
                otherAreaComp->getShape() == Area3DShape::SPHERE) {
                // Sphere-sphere overlap: distance between centers <= sum of radii
                float otherRadius = otherAreaComp->getRadius();
                float combinedRadius = radius + otherRadius;
                isInside = (distance <= combinedRadius);
            } else {
                // Point-in-sphere or sphere vs physics body: just check if point is within radius
                isInside = (distance <= radius);
            }
        } else if (shapeType == Area3DShape::CAPSULE || shapeType == Area3DShape::CYLINDER) {
            // Capsule/Cylinder check (simplified - check distance in XZ plane and Y)
            glm::vec3 localPos = objectPos - areaPos;
            float horizontalDist = glm::length(glm::vec2(localPos.x, localPos.z));
            float verticalPos = localPos.y;
            
            if (shapeType == Area3DShape::CAPSULE) {
                // Capsule: Bullet convention - height is distance between cap centers
                // Total height = height + 2*radius
                // Cylinder goes from -height/2 to +height/2
                // Sphere caps centered at -height/2 and +height/2
                float halfHeight = height * 0.5f;
                float bottomCapCenter = -halfHeight;
                float topCapCenter = halfHeight;
                
                // Check if within the cylinder part
                if (verticalPos >= bottomCapCenter && verticalPos <= topCapCenter) {
                    isInside = (horizontalDist <= radius);
                } else if (verticalPos > topCapCenter) {
                    // Top sphere cap (center at +height/2)
                    float verticalOffset = verticalPos - topCapCenter;
                    float distanceFromTopCapCenter = glm::length(glm::vec2(horizontalDist, verticalOffset));
                    isInside = (distanceFromTopCapCenter <= radius);
                } else {
                    // Bottom sphere cap (center at -height/2)
                    float verticalOffset = verticalPos - bottomCapCenter;
                    float distanceFromBottomCapCenter = glm::length(glm::vec2(horizontalDist, verticalOffset));
                    isInside = (distanceFromBottomCapCenter <= radius);
                }
            } else {
                // Cylinder: height in Y, radius in XZ
                float halfHeight = height * 0.5f;
                isInside = (horizontalDist <= radius && 
                           verticalPos >= -halfHeight && 
                           verticalPos <= halfHeight);
            }
        }
        
        if (isInside) {
            // Get object identifier
            std::string bodyName = "";
            void* userData = nullptr;
            
            // Try to get name from PhysicsComponent first
            PhysicsComponent* physicsComp = static_cast<PhysicsComponent*>(obj->getUserPointer());
            if (physicsComp && physicsComp->getOwner()) {
                bodyName = physicsComp->getOwner()->getName();
                userData = physicsComp;
            } else {
                // Try to get from Area3DComponent
                Area3DComponent* areaComp = static_cast<Area3DComponent*>(obj->getUserPointer());
                if (areaComp && areaComp->getOwner()) {
                    // Skip if this is our own component (triple-check to prevent self-detection)
                    if (areaComp == this) {
                        continue; // Skip self - should never reach here due to earlier check
                    }
                    bodyName = areaComp->getOwner()->getName();
                    userData = areaComp;
                } else {
                    // Fallback to generic name
                    bodyName = "Object_" + std::to_string(i);
                }
            }
            
            // Check if this object matches our detection tags (if any specified)
            if (detectionTags.empty() || 
                std::find(detectionTags.begin(), detectionTags.end(), bodyName) != detectionTags.end()) {
                bodiesInArea.insert(bodyName);
            }
        }
    }
}

void Area3DComponent::handleCollisionEvents() {
    // Handle enter events
    for (const auto& bodyName : bodiesInArea) {
        if (previousBodiesInArea.find(bodyName) == previousBodiesInArea.end()) {
            // Body entered the area
            void* userData = nullptr;
            // Try to find the user data for this body
            btDiscreteDynamicsWorld* world = PhysicsManager::getInstance().getDynamicsWorld();
            if (world) {
                for (int i = 0; i < world->getNumCollisionObjects(); i++) {
                    btCollisionObject* obj = world->getCollisionObjectArray()[i];
                    PhysicsComponent* physicsComp = static_cast<PhysicsComponent*>(obj->getUserPointer());
                    Area3DComponent* areaComp = static_cast<Area3DComponent*>(obj->getUserPointer());
                    if ((physicsComp && physicsComp->getOwner() && physicsComp->getOwner()->getName() == bodyName) ||
                        (areaComp && areaComp->getOwner() && areaComp->getOwner()->getName() == bodyName)) {
                        userData = obj->getUserPointer();
                        break;
                    }
                }
            }
            
            if (onBodyEntered) {
                onBodyEntered(bodyName, userData);
            }
        }
    }
    
    // Handle exit events
    for (const auto& bodyName : previousBodiesInArea) {
        if (bodiesInArea.find(bodyName) == bodiesInArea.end()) {
            // Body exited the area
            void* userData = nullptr;
            // Try to find the user data for this body (simplified - may not find it if already removed)
            btDiscreteDynamicsWorld* world = PhysicsManager::getInstance().getDynamicsWorld();
            if (world) {
                for (int i = 0; i < world->getNumCollisionObjects(); i++) {
                    btCollisionObject* obj = world->getCollisionObjectArray()[i];
                    PhysicsComponent* physicsComp = static_cast<PhysicsComponent*>(obj->getUserPointer());
                    Area3DComponent* areaComp = static_cast<Area3DComponent*>(obj->getUserPointer());
                    if ((physicsComp && physicsComp->getOwner() && physicsComp->getOwner()->getName() == bodyName) ||
                        (areaComp && areaComp->getOwner() && areaComp->getOwner()->getName() == bodyName)) {
                        userData = obj->getUserPointer();
                        break;
                    }
                }
            }
            
            if (onBodyExited) {
                onBodyExited(bodyName, userData);
            }
        }
    }
    
    // Handle stay events
    for (const auto& bodyName : bodiesInArea) {
        void* userData = nullptr;
        // Try to find the user data for this body
        btDiscreteDynamicsWorld* world = PhysicsManager::getInstance().getDynamicsWorld();
        if (world) {
            for (int i = 0; i < world->getNumCollisionObjects(); i++) {
                btCollisionObject* obj = world->getCollisionObjectArray()[i];
                PhysicsComponent* physicsComp = static_cast<PhysicsComponent*>(obj->getUserPointer());
                Area3DComponent* areaComp = static_cast<Area3DComponent*>(obj->getUserPointer());
                if ((physicsComp && physicsComp->getOwner() && physicsComp->getOwner()->getName() == bodyName) ||
                    (areaComp && areaComp->getOwner() && areaComp->getOwner()->getName() == bodyName)) {
                    userData = obj->getUserPointer();
                    break;
                }
            }
        }
        
        if (onBodyStayed) {
            onBodyStayed(bodyName, userData);
        }
    }
}

glm::vec3 Area3DComponent::getWorldPosition() const {
    if (!owner) {
        return glm::vec3(0.0f);
    }
    
    // Force recalculation by getting fresh world matrix (not cached)
    glm::mat4 worldMatrix = owner->getWorldMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    
    // Debug output if position seems wrong
    #ifndef VITA_BUILD
    if (worldPos.y > 500.0f || worldPos.y < -100.0f) {
        std::cout << "Area3DComponent::getWorldPosition: WARNING - Suspicious Y position: " << worldPos.y 
                  << " for node: " << owner->getName() << std::endl;
        if (owner->getParent()) {
            std::cout << "  Parent: " << owner->getParent()->getName() << std::endl;
            auto parentWorld = owner->getParent()->getWorldMatrix();
            glm::vec3 parentWorldPos = glm::vec3(parentWorld[3]);
            std::cout << "  Parent world position: (" << parentWorldPos.x << ", " << parentWorldPos.y << ", " << parentWorldPos.z << ")" << std::endl;
        }
        auto localPos = owner->getTransform().getPosition();
        std::cout << "  Local position: (" << localPos.x << ", " << localPos.y << ", " << localPos.z << ")" << std::endl;
    }
    #endif
    
    return worldPos;
}

glm::mat4 Area3DComponent::getWorldTransformMatrix() const {
    if (!owner) {
        return glm::mat4(1.0f);
    }
    
    // Always get fresh world matrix - don't cache it
    return owner->getWorldMatrix();
}

btCollisionShape* Area3DComponent::createBulletCollisionShape() {
    switch (shapeType) {
        case Area3DShape::BOX:
            return PhysicsManager::getInstance().createBoxShape(dimensions * 0.5f);
            
        case Area3DShape::SPHERE:
            return PhysicsManager::getInstance().createSphereShape(radius);
            
        case Area3DShape::CAPSULE:
            return PhysicsManager::getInstance().createCapsuleShape(radius, height);
            
        case Area3DShape::CYLINDER:
            return PhysicsManager::getInstance().createCylinderShape(glm::vec3(radius, height * 0.5f, radius));
            
        case Area3DShape::PLANE:
            // Plane is a special case - use dimensions for plane normal/constant
            return PhysicsManager::getInstance().createPlaneShape(glm::vec3(0, 1, 0), 0.0f);
            
        default:
            return PhysicsManager::getInstance().createBoxShape(glm::vec3(0.5f));
    }
}

void Area3DComponent::registerWithGroup() {
    if (group.empty()) {
        return;
    }
    
    // Remove from any existing group first
    unregisterFromGroup();
    
    // Add to group registry
    groupRegistry[group].push_back(this);
}

void Area3DComponent::unregisterFromGroup() {
    if (group.empty()) {
        return;
    }
    
    auto it = groupRegistry.find(group);
    if (it != groupRegistry.end()) {
        auto& components = it->second;
        components.erase(std::remove(components.begin(), components.end(), this), components.end());
        
        // Remove group if empty
        if (components.empty()) {
            groupRegistry.erase(it);
        }
    }
}

void Area3DComponent::renderDebugWireframe(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
#ifdef EDITOR_BUILD
    if (!owner || !showDebugShape) {
        return;
    }
    
    // Create wireframe mesh based on shape type
    std::shared_ptr<Mesh> wireframeMesh;
    
    switch (shapeType) {
        case Area3DShape::BOX:
            wireframeMesh = Mesh::createWireframeBox(dimensions * 0.5f);
            break;
            
        case Area3DShape::SPHERE:
            wireframeMesh = Mesh::createWireframeSphere(radius, 16);
            break;
            
        case Area3DShape::CAPSULE:
            wireframeMesh = Mesh::createWireframeCapsule(radius, height, 16);
            break;
            
        case Area3DShape::CYLINDER:
            wireframeMesh = Mesh::createWireframeCylinder(radius, height, 16);
            break;
            
        case Area3DShape::PLANE:
            wireframeMesh = Mesh::createWireframePlane(dimensions.x, dimensions.z);
            break;
            
        default:
            return; // Unknown shape type
    }
    
    if (!wireframeMesh) {
        return;
    }
    
    // Get the world transform of this node (includes parent transforms)
    glm::mat4 worldTransform = owner->getWorldMatrix();
    
    // Create debug material (cyan wireframe for Area3D)
    static std::shared_ptr<Material> debugMaterial = nullptr;
    static std::shared_ptr<Shader> debugShader = nullptr;
    
    if (!debugMaterial) {
        debugMaterial = std::make_shared<Material>();
        debugMaterial->setColor(glm::vec3(0.0f, 1.0f, 1.0f)); // Cyan wireframe
        
        debugShader = std::make_shared<Shader>();
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
        }
    }
    
    // Store OpenGL state
    GLint currentPolygonMode[2];
    glGetIntegerv(GL_POLYGON_MODE, currentPolygonMode);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    // Bind and render wireframe mesh
    wireframeMesh->bind();
    
    // Apply the debug material
    debugMaterial->apply();
    
    // Set transformation matrices
    auto shader = debugMaterial->getShader();
    if (shader) {
        shader->setMat4("modelMatrix", worldTransform);
        shader->setMat4("viewMatrix", viewMatrix);
        shader->setMat4("projectionMatrix", projectionMatrix);
        shader->setVec3("u_Color", debugMaterial->getColor());
    }
    
    // Draw the wireframe mesh
    wireframeMesh->draw();
    
    wireframeMesh->unbind();
    
    // Restore OpenGL state
    glPolygonMode(GL_FRONT_AND_BACK, currentPolygonMode[0]);
#endif
}

void Area3DComponent::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Area3D Component", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Shape type
        const char* shapeTypes[] = { "Box", "Sphere", "Capsule", "Cylinder", "Plane" };
        int currentShape = static_cast<int>(shapeType);
        if (ImGui::Combo("Shape Type", &currentShape, shapeTypes, 5)) {
            setShape(static_cast<Area3DShape>(currentShape));
        }
        
        // Dimensions (for box)
        if (shapeType == Area3DShape::BOX) {
            glm::vec3 dims = dimensions;
            if (ImGui::DragFloat3("Dimensions", &dims.x, 0.1f)) {
                setDimensions(dims);
            }
        }
        
        // Radius (for sphere, capsule, cylinder)
        if (shapeType == Area3DShape::SPHERE || 
            shapeType == Area3DShape::CAPSULE || 
            shapeType == Area3DShape::CYLINDER) {
            float rad = radius;
            if (ImGui::DragFloat("Radius", &rad, 0.1f)) {
                setRadius(rad);
            }
        }
        
        // Height (for capsule and cylinder)
        if (shapeType == Area3DShape::CAPSULE || shapeType == Area3DShape::CYLINDER) {
            float h = height;
            if (ImGui::DragFloat("Height", &h, 0.1f)) {
                setHeight(h);
            }
        }
        
        // Group property
        ImGui::Separator();
        ImGui::Text("Group");
        char groupBuffer[256] = "";
        if (!group.empty()) {
            strncpy(groupBuffer, group.c_str(), sizeof(groupBuffer) - 1);
        }
        if (ImGui::InputText("Group Name", groupBuffer, sizeof(groupBuffer))) {
            setGroup(std::string(groupBuffer));
        }
        if (ImGui::Button("Clear Group")) {
            setGroup("");
        }
        
        // Monitor mode
        ImGui::Separator();
        bool monitor = monitorEnabled;
        if (ImGui::Checkbox("Monitor", &monitor)) {
            setMonitorMode(monitor);
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
        ImGui::Text("Bodies in area: %zu", getBodyCount());
        
        if (ImGui::CollapsingHeader("Bodies in Area")) {
            auto bodies = getBodiesInArea();
            for (const auto& bodyName : bodies) {
                ImGui::BulletText("%s", bodyName.c_str());
            }
        }
        
        // Group info
        if (!group.empty()) {
            ImGui::Separator();
            ImGui::Text("Group: %s", group.c_str());
            auto componentsInGroup = getComponentsInGroup(group);
            ImGui::Text("Components in group: %zu", componentsInGroup.size());
        }
    }
#endif
}

} // namespace GameEngine

