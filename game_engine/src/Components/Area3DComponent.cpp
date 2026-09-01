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
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

// ImGui for editor
#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

namespace GameEngine {

// Static group registry initialization
std::unordered_map<std::string, std::vector<Area3DComponent*>> Area3DComponent::groupRegistry;
std::unordered_set<Area3DComponent*> Area3DComponent::liveInstances;

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
    , lastShapeScale(1.0f, 1.0f, 1.0f)
    , showDebugShape(false)
{
    liveInstances.insert(this);
}

Area3DComponent::~Area3DComponent() {
    destroy();
    liveInstances.erase(this);
    notifyBodyComponentDestroyed(this);
}

void Area3DComponent::notifyBodyComponentDestroyed(Component* component) {
    if (!component) {
        return;
    }
    for (Area3DComponent* area : liveInstances) {
        if (area == component) {
            continue;
        }
        area->bodiesInArea.erase(component);
        area->previousBodiesInArea.erase(component);
    }
}

void Area3DComponent::start() {
    if (!ghostObject) {
        createCollisionShape();
        createGhostObject();
    }

    registerWithGroup();

    // Initialize state tracking
    bodiesInArea.clear();
    previousBodiesInArea.clear();
}

void Area3DComponent::update(float deltaTime) {
    if (!ghostObject || !owner || !monitorEnabled) {
        return;
    }
    
    glm::vec3 currentScale = owner->getWorldScale();
    const float kScaleEpsilon = 0.0001f;
    if (std::abs(currentScale.x - lastShapeScale.x) > kScaleEpsilon ||
        std::abs(currentScale.y - lastShapeScale.y) > kScaleEpsilon ||
        std::abs(currentScale.z - lastShapeScale.z) > kScaleEpsilon) {
        updateCollisionShape();
    }

    // Update the ghost object's transform to match the node's world transform
    // Force recalculation of world matrix each frame to ensure it's up-to-date
    glm::mat4 worldMatrix = getWorldTransformMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    glm::quat worldRot = owner->getWorldRotation();
    
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
    
    btVector3 aabbMin, aabbMax;
    if (collisionShape) {
        collisionShape->getAabb(transform, aabbMin, aabbMax);
        btBroadphaseInterface* broadphase = PhysicsManager::getInstance().getDynamicsWorld()->getBroadphase();
        if (broadphase && ghostObject->getBroadphaseHandle()) {
            broadphase->setAabb(ghostObject->getBroadphaseHandle(), aabbMin, aabbMax, 
                              PhysicsManager::getInstance().getDynamicsWorld()->getDispatcher());
        }
    }
    
    // Perform collision detection
    performCollisionDetection();
    
    // Handle collision events
    handleCollisionEvents();
}

void Area3DComponent::render(IRenderer& renderer) {
    // Area3D components are trigger zones - they don't need regular rendering
    // Debug wireframe rendering is handled by EditorSystem::renderNodeDirectly()
}

void Area3DComponent::destroy() {
    unregisterFromGroup();
    destroyGhostObject();
}

void Area3DComponent::suspend() {
    destroy();
}

void Area3DComponent::resume() {
    start();
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
    for (Component* body : bodiesInArea) {
        if (body && body->getOwner() && body->getOwner()->getName() == bodyName) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> Area3DComponent::getBodiesInArea() const {
    std::vector<std::string> names;
    names.reserve(bodiesInArea.size());
    for (Component* body : bodiesInArea) {
        if (body && body->getOwner()) {
            names.push_back(body->getOwner()->getName());
        }
    }
    return names;
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
    if (owner) {
        lastShapeScale = owner->getWorldScale();
    }
}

void Area3DComponent::createGhostObject() {
    if (!collisionShape) {
        return;
    }

    // Never overwrite a live ghost
    if (ghostObject) {
        return;
    }


    // Create pair caching ghost object for trigger detection (sensor/trigger zone)
    // This type of object detects collisions but doesn't generate contact responses
    // Optimized for PS Vita - uses pair caching for efficient overlap detection
    btPairCachingGhostObject* pairCachingGhost = new btPairCachingGhostObject();
    pairCachingGhost->setCollisionShape(collisionShape);
    // CF_NO_CONTACT_RESPONSE tells Bullet this is a sensor - no collision response
    pairCachingGhost->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    // Also set activation state to prevent any physics processing
    pairCachingGhost->setActivationState(DISABLE_DEACTIVATION);
    
    // Set initial transform
    glm::mat4 worldMatrix = getWorldTransformMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    glm::quat worldRot = owner->getWorldRotation();
    
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z));
    transform.setRotation(btQuaternion(worldRot.x, worldRot.y, worldRot.z, worldRot.w));
    pairCachingGhost->setWorldTransform(transform);
    
    // Store pointer to this component in user data
    pairCachingGhost->setUserPointer(this);
    
    // Cast to base class for storage
    ghostObject = pairCachingGhost;
    
    // Add to physics world for collision detection (as a sensor/trigger)
    PhysicsManager::getInstance().addSensorObject(ghostObject);
}

void Area3DComponent::destroyGhostObject() {
    if (ghostObject) {
        PhysicsManager::getInstance().removeSensorObject(ghostObject);
        // Cast back to btPairCachingGhostObject for proper deletion
        btPairCachingGhostObject* pairCachingGhost = dynamic_cast<btPairCachingGhostObject*>(ghostObject);
        if (pairCachingGhost) {
            delete pairCachingGhost;
        } else {
            delete ghostObject;
        }
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
    PhysicsManager::getInstance().removeSensorObject(ghostObject);
    
    // Delete old collision shape
    if (collisionShape) {
        delete collisionShape;
    }
    
    // Create new collision shape
    collisionShape = createBulletCollisionShape();
    ghostObject->setCollisionShape(collisionShape);
    if (owner) {
        lastShapeScale = owner->getWorldScale();
    }
    
    // Add back to world as sensor/trigger
    PhysicsManager::getInstance().addSensorObject(ghostObject);
}

void Area3DComponent::performCollisionDetection() {
    if (!ghostObject || !owner) {
        return;
    }
    
    // Save previous state (wap, not copy)
    previousBodiesInArea.swap(bodiesInArea);
    bodiesInArea.clear();
    
    // Use the ghost object's built-in overlap detection (optimized for PS Vita)
    // The btGhostPairCallback automatically tracks overlapping objects via AABB
    // We use AABB as a fast culling pass, then do precise shape-based checks
    btGhostObject* ghost = btGhostObject::upcast(ghostObject);
    if (!ghost) {
        return;
    }
    
    // Get area world position for precise shape checks
    glm::vec3 areaPos = getWorldPosition();
    glm::quat areaRot = owner->getWorldRotation();
    glm::vec3 areaScale = owner->getWorldScale();
    
    // Get all overlapping objects directly from the ghost object (AABB-based, fast)
    // Then do precise shape-based checks to filter out false positives
    int numOverlapping = ghost->getNumOverlappingObjects();
    for (int i = 0; i < numOverlapping; i++) {
        btCollisionObject* obj = ghost->getOverlappingObject(i);
        if (!obj) {
            continue;
        }
        
        // Skip if this is our own ghost object (shouldn't happen, but safety check)
        if (obj == ghostObject) {
            continue;
        }
        
        // Resolve the owning component through the common base; user pointers
        // hold either a PhysicsComponent or an Area3DComponent.
        Component* comp = static_cast<Component*>(obj->getUserPointer());
        if (!comp || !comp->getOwner() || comp == this) {
            continue;
        }
        Area3DComponent* areaComp =
            (comp->getTypeName() == "Area3DComponent") ? static_cast<Area3DComponent*>(comp) : nullptr;
        
        // Get the object's world position
        btTransform objTransform = obj->getWorldTransform();
        btVector3 objPos = objTransform.getOrigin();
        glm::vec3 objectPos = glm::vec3(objPos.x(), objPos.y(), objPos.z());
        
        // Check if object is actually within the precise detection area shape
        bool isInside = false;
        glm::vec3 localPos = glm::inverse(areaRot) * (objectPos - areaPos);
        
        if (shapeType == Area3DShape::BOX) {
            glm::vec3 halfExtents = dimensions * areaScale * 0.5f;
            isInside = (localPos.x >= -halfExtents.x && localPos.x <= halfExtents.x &&
                        localPos.y >= -halfExtents.y && localPos.y <= halfExtents.y &&
                        localPos.z >= -halfExtents.z && localPos.z <= halfExtents.z);
        } else if (shapeType == Area3DShape::SPHERE) {
            float maxScale = std::max({areaScale.x, areaScale.y, areaScale.z});
            
            if (areaComp && areaComp->getShape() == Area3DShape::SPHERE) {
                float otherRadius = areaComp->getRadius();
                float otherMaxScale = std::max({
                    areaComp->getOwner()->getWorldScale().x,
                    areaComp->getOwner()->getWorldScale().y,
                    areaComp->getOwner()->getWorldScale().z
                });
                float combinedRadius = radius * maxScale + otherRadius * otherMaxScale;
                isInside = (glm::length(objectPos - areaPos) <= combinedRadius);
            } else {
                isInside = (glm::length(localPos) <= radius * maxScale);
            }
        } else if (shapeType == Area3DShape::CAPSULE || shapeType == Area3DShape::CYLINDER) {
            float horizontalDist = glm::length(glm::vec2(localPos.x, localPos.z));
            float verticalPos = localPos.y;
            float scaledRadius = radius * std::max(areaScale.x, areaScale.z);
            float scaledHeight = height * areaScale.y;
            
            if (shapeType == Area3DShape::CAPSULE) {
                float halfHeight = scaledHeight * 0.5f;
                float bottomCapCenter = -halfHeight;
                float topCapCenter = halfHeight;
                
                if (verticalPos >= bottomCapCenter && verticalPos <= topCapCenter) {
                    isInside = (horizontalDist <= scaledRadius);
                } else if (verticalPos > topCapCenter) {
                    float verticalOffset = verticalPos - topCapCenter;
                    float distanceFromTopCapCenter = glm::length(glm::vec2(horizontalDist, verticalOffset));
                    isInside = (distanceFromTopCapCenter <= scaledRadius);
                } else {
                    float verticalOffset = verticalPos - bottomCapCenter;
                    float distanceFromBottomCapCenter = glm::length(glm::vec2(horizontalDist, verticalOffset));
                    isInside = (distanceFromBottomCapCenter <= scaledRadius);
                }
            } else {
                float halfHeight = scaledHeight * 0.5f;
                isInside = (horizontalDist <= scaledRadius && 
                           verticalPos >= -halfHeight && 
                           verticalPos <= halfHeight);
            }
        }
        
        if (isInside) {
            // Check if this object matches our detection tags (if any specified)
            if (detectionTags.empty()) {
                bodiesInArea.insert(comp);
            } else {
                const std::string& bodyName = comp->getOwner()->getName();
                if (std::find(detectionTags.begin(), detectionTags.end(), bodyName) != detectionTags.end()) {
                    bodiesInArea.insert(comp);
                }
            }
        }
    }
}

void Area3DComponent::handleCollisionEvents() {
    if (!onBodyEntered && !onBodyExited && !onBodyStayed) {
        return;
    }

    auto fire = [](const std::function<void(const std::string&, void*)>& callback, Component* body) {
        if (!body || !body->getOwner()) {
            return;
        }
        callback(body->getOwner()->getName(), body);
    };

    if (onBodyEntered) {
        for (Component* body : bodiesInArea) {
            if (previousBodiesInArea.find(body) == previousBodiesInArea.end()) {
                fire(onBodyEntered, body);
            }
        }
    }

    if (onBodyExited) {
        for (Component* body : previousBodiesInArea) {
            if (bodiesInArea.find(body) == bodiesInArea.end()) {
                fire(onBodyExited, body);
            }
        }
    }

    if (onBodyStayed) {
        for (Component* body : bodiesInArea) {
            fire(onBodyStayed, body);
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
    glm::vec3 worldScale(1.0f);
    if (owner) {
        worldScale = owner->getWorldScale();
    }
    
    switch (shapeType) {
        case Area3DShape::BOX:
            return PhysicsManager::getInstance().createBoxShape(dimensions * worldScale * 0.5f);
            
        case Area3DShape::SPHERE: {
            float maxScale = std::max({worldScale.x, worldScale.y, worldScale.z});
            return PhysicsManager::getInstance().createSphereShape(radius * maxScale);
        }
            
        case Area3DShape::CAPSULE: {
            float radiusScale = std::max(worldScale.x, worldScale.z);
            return PhysicsManager::getInstance().createCapsuleShape(radius * radiusScale, height * worldScale.y);
        }
            
        case Area3DShape::CYLINDER:
            return PhysicsManager::getInstance().createCylinderShape(glm::vec3(
                radius * worldScale.x,
                height * worldScale.y * 0.5f,
                radius * worldScale.z));
            
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

