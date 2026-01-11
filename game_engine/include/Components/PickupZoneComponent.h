#ifndef PICKUP_ZONE_COMPONENT_H
#define PICKUP_ZONE_COMPONENT_H

#include "Component.h"
#include "Platform/VitaMath.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>
#include <string>

class btCollisionObject;
class btCollisionShape;
class btGhostObject;

namespace GameEngine {
    class PhysicsComponent;
}

namespace GameEngine {

enum class PickupZoneShape {
    BOX,
    SPHERE,
    CAPSULE
};

class PickupZoneComponent : public Component {
public:
    PickupZoneComponent();
    virtual ~PickupZoneComponent();
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void destroy() override;
    
    COMPONENT_TYPE(PickupZoneComponent);
    
    void setShape(PickupZoneShape shape);
    void setDimensions(const glm::vec3& dimensions);
    void setRadius(float radius);
    void setHeight(float height);
    
    void setDetectionTags(const std::vector<std::string>& tags);
    void setDetectionRadius(float radius);
    void setContinuousDetection(bool enabled);
    
    void setOnEnterCallback(std::function<void(const std::string&)> callback);
    void setOnExitCallback(std::function<void(const std::string&)> callback);
    void setOnStayCallback(std::function<void(const std::string&)> callback);
    
    bool isObjectInZone(const std::string& objectTag) const;
    std::vector<std::string> getObjectsInZone() const;
    size_t getObjectCount() const;
    
    void setShowDebugShape(bool show);
    bool getShowDebugShape() const;
    
    virtual void drawInspector() override;

private:
    PickupZoneShape shapeType;
    glm::vec3 dimensions;
    float radius;
    float height;
    
    std::vector<std::string> detectionTags;
    float detectionRadius;
    bool continuousDetection;
    
    btCollisionObject* ghostObject;
    btCollisionShape* collisionShape;
    
    std::function<void(const std::string&)> onEnterCallback;
    std::function<void(const std::string&)> onExitCallback;
    std::function<void(const std::string&)> onStayCallback;
    
    std::vector<std::string> objectsInZone;
    std::vector<std::string> previousObjectsInZone;
    
    bool showDebugShape;
    
    void createCollisionShape();
    void createGhostObject();
    void destroyGhostObject();
    void updateCollisionShape();
    void performCollisionDetection();
    void handleCollisionEvents();
    
    glm::vec3 getWorldPosition() const;
    btCollisionShape* createBulletCollisionShape();
};

} // namespace GameEngine

#endif // PICKUP_ZONE_COMPONENT_H
