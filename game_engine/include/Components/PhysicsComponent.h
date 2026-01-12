#ifndef PHYSICS_COMPONENT_H
#define PHYSICS_COMPONENT_H

#include "Components/Component.h"
#include <glm/glm.hpp>
#include <memory>
#include <functional>

namespace GameEngine {
    class Renderer;
    class Material;
}

class btRigidBody;
class btCollisionShape;
class btMotionState;

namespace GameEngine {

enum class CollisionShapeType {
    BOX,
    SPHERE,
    CAPSULE,
    CYLINDER,
    PLANE
};

enum class PhysicsBodyType {
    STATIC,     // Mass = 0, doesn't move
    DYNAMIC,    // Mass > 0, affected by forces
    KINEMATIC   // Mass = 0, but can be moved programmatically
};

class PhysicsComponent : public Component {
public:
    PhysicsComponent();
    virtual ~PhysicsComponent();
    
    COMPONENT_TYPE(PhysicsComponent)
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void destroy() override;
    virtual void render(Renderer& renderer) override;
    
    void setCollisionShape(CollisionShapeType shapeType, const glm::vec3& dimensions = glm::vec3(1.0f));
    CollisionShapeType getCollisionShapeType() const { return collisionShapeType; }
    
    void setBodyType(PhysicsBodyType type);
    PhysicsBodyType getBodyType() const { return bodyType; }
    
    void setMass(float mass);
    float getMass() const { return mass; }
    
    void setFriction(float friction);
    float getFriction() const { return friction; }
    
    void setRestitution(float restitution);
    float getRestitution() const { return restitution; }
    
    void setLinearDamping(float damping);
    float getLinearDamping() const { return linearDamping; }
    
    void setAngularDamping(float damping);
    float getAngularDamping() const { return angularDamping; }
    
    void setLinearVelocity(const glm::vec3& velocity);
    glm::vec3 getLinearVelocity() const;
    
    void setAngularVelocity(const glm::vec3& velocity);
    glm::vec3 getAngularVelocity() const;
    
    void setGravityEnabled(bool enabled);
    bool isGravityEnabled() const;
    
    void setAngularFactor(const glm::vec3& factor);
    glm::vec3 getAngularFactor() const;
    
    void setLinearFactor(const glm::vec3& factor);
    glm::vec3 getLinearFactor() const;
    
    void applyForce(const glm::vec3& force, const glm::vec3& point = glm::vec3(0.0f));
    void applyImpulse(const glm::vec3& impulse, const glm::vec3& point = glm::vec3(0.0f));
    void applyTorque(const glm::vec3& torque);
    void applyTorqueImpulse(const glm::vec3& torque);
    
    void syncTransformFromPhysics();
    void syncTransformToPhysics();
    
    void forceUpdateCollisionShape();
    
    bool isColliding() const;
    void setCollisionCallback(std::function<void(PhysicsComponent*)> callback);
    
    void setShowCollisionShape(bool show);
    bool getShowCollisionShape() const { return showCollisionShape; }
    
    virtual void drawInspector() override;
    
    virtual void drawCollisionShape() const;
    
    void renderDebugShape(Material& debugMaterial,
                         const glm::mat4& viewMatrix = glm::mat4(1.0f),
                         const glm::mat4& projectionMatrix = glm::mat4(1.0f));
    
    btRigidBody* getRigidBody() const { return rigidBody; }
    btCollisionShape* getCollisionShape() const { return collisionShape; }
    
private:
    CollisionShapeType collisionShapeType;
    glm::vec3 shapeDimensions;
    
    PhysicsBodyType bodyType;
    float mass;
    float friction;
    float restitution;
    float linearDamping;
    float angularDamping;
    
    bool gravityEnabled;
    
    btRigidBody* rigidBody;
    btCollisionShape* collisionShape;
    btMotionState* motionState;
    
    bool isCollidingFlag;
    std::function<void(PhysicsComponent*)> collisionCallback;
    
    bool showCollisionShape;
    
    glm::mat4 lastWorldTransform;
    
    bool destroyed;
    
    void createRigidBody();
    void destroyRigidBody();
    void updateCollisionShape();
    btCollisionShape* createBulletCollisionShape();
    
    glm::vec3 bulletToGlm(const void* bulletVec) const;
    void glmToBullet(const glm::vec3& glmVec, void* bulletVec) const;
};

} // namespace GameEngine

#endif // PHYSICS_COMPONENT_H
