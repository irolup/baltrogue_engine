#ifndef AREA_3D_COMPONENT_H
#define AREA_3D_COMPONENT_H

#include "Components/Component.h"
#include "Components/PhysicsComponent.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class btCollisionObject;
class btCollisionShape;
class btGhostObject;
class btDiscreteDynamicsWorld;

namespace GameEngine {

enum class Area3DShape {
    BOX,
    SPHERE,
    CAPSULE,
    CYLINDER,
    PLANE
};

class Area3DComponent : public Component {
public:
    Area3DComponent();
    virtual ~Area3DComponent();
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void render(Renderer& renderer) override;
    virtual void destroy() override;
    
    COMPONENT_TYPE(Area3DComponent);
    
    void setShape(Area3DShape shape);
    Area3DShape getShape() const { return shapeType; }
    void setDimensions(const glm::vec3& dimensions);
    glm::vec3 getDimensions() const { return dimensions; }
    void setRadius(float radius);
    float getRadius() const { return radius; }
    void setHeight(float height);
    float getHeight() const { return height; }
    
    void setGroup(const std::string& group);
    std::string getGroup() const { return group; }
    bool hasGroup() const { return !group.empty(); }
    
    void setDetectionTags(const std::vector<std::string>& tags);
    std::vector<std::string> getDetectionTags() const { return detectionTags; }
    void setMonitorMode(bool enabled) { monitorEnabled = enabled; }
    bool getMonitorMode() const { return monitorEnabled; }
    
    void setOnBodyEntered(std::function<void(const std::string&, void*)> callback) { onBodyEntered = callback; }
    void setOnBodyExited(std::function<void(const std::string&, void*)> callback) { onBodyExited = callback; }
    void setOnBodyStayed(std::function<void(const std::string&, void*)> callback) { onBodyStayed = callback; }
    
    bool isBodyInArea(const std::string& bodyName) const;
    std::vector<std::string> getBodiesInArea() const;
    size_t getBodyCount() const;
    
    static std::vector<Area3DComponent*> getComponentsInGroup(const std::string& groupName);
    
    void setShowDebugShape(bool show);
    bool getShowDebugShape() const { return showDebugShape; }
    
    void renderDebugWireframe(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
    virtual void drawInspector() override;

private:
    Area3DShape shapeType;
    glm::vec3 dimensions;
    float radius;
    float height;
    
    std::string group;
    
    std::vector<std::string> detectionTags;
    bool monitorEnabled;
    
    btCollisionObject* ghostObject;
    btCollisionShape* collisionShape;
    
    std::function<void(const std::string&, void*)> onBodyEntered;
    std::function<void(const std::string&, void*)> onBodyExited;
    std::function<void(const std::string&, void*)> onBodyStayed;
    
    std::unordered_set<std::string> bodiesInArea;
    std::unordered_set<std::string> previousBodiesInArea;
    
    bool showDebugShape;
    
    static std::unordered_map<std::string, std::vector<Area3DComponent*>> groupRegistry;
    
    void createCollisionShape();
    void createGhostObject();
    void destroyGhostObject();
    void updateCollisionShape();
    void performCollisionDetection();
    void handleCollisionEvents();
    
    glm::vec3 getWorldPosition() const;
    glm::mat4 getWorldTransformMatrix() const;
    btCollisionShape* createBulletCollisionShape();
    void registerWithGroup();
    void unregisterFromGroup();
};

} // namespace GameEngine

#endif // AREA_3D_COMPONENT_H

