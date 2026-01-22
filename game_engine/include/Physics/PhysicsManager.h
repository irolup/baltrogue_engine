#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#include <memory>
#include <vector>
#include <glm/glm.hpp>

class btDiscreteDynamicsWorld;
class btCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btConstraintSolver;
class btRigidBody;
class btCollisionShape;
class btMotionState;
class btGhostPairCallback;
class btITaskScheduler;

// Forward declarations for rendering
namespace GameEngine {
    class Renderer;
    class Material;
}

namespace GameEngine {

class PhysicsComponent;


class PhysicsManager {
public:
    static PhysicsManager& getInstance();
    
    // Physics system lifecycle
    bool initialize();
    void shutdown();
    void update(float deltaTime);
    
    
    // Physics world management
    btDiscreteDynamicsWorld* getDynamicsWorld() const { return dynamicsWorld; }
    
    // Rigid body management
    void addRigidBody(btRigidBody* body);
    void removeRigidBody(btRigidBody* body);
    
    // Physics properties
    void setGravity(const glm::vec3& gravity);
    glm::vec3 getGravity() const;
    
    // Collision shape creation helpers
    btCollisionShape* createBoxShape(const glm::vec3& halfExtents);
    btCollisionShape* createSphereShape(float radius);
    btCollisionShape* createCapsuleShape(float radius, float height);
    btCollisionShape* createCylinderShape(const glm::vec3& halfExtents);
    btCollisionShape* createPlaneShape(const glm::vec3& normal, float constant);
    
    // Debug drawing
    void setDebugDrawEnabled(bool enabled);
    bool isDebugDrawEnabled() const;
    
    // Debug rendering
    void renderDebugShapes(Material& debugMaterial, 
                          const glm::mat4& viewMatrix = glm::mat4(1.0f), 
                          const glm::mat4& projectionMatrix = glm::mat4(1.0f));
    
    // Physics component registration
    void registerPhysicsComponent(PhysicsComponent* component);
    void unregisterPhysicsComponent(PhysicsComponent* component);
    
    
private:
    PhysicsManager();
    ~PhysicsManager();
    
    // Bullet physics objects
    btDiscreteDynamicsWorld* dynamicsWorld;
    btCollisionConfiguration* collisionConfiguration;
    btCollisionDispatcher* dispatcher;
    btBroadphaseInterface* broadphase;
    btConstraintSolver* solver;
    btGhostPairCallback* ghostPairCallback;
    btITaskScheduler* scheduler;
    
    // Physics components
    std::vector<PhysicsComponent*> physicsComponents;
    
    // Debug drawing
    bool debugDrawEnabled;

    void cleanupPhysicsObjects();
    
};

} // namespace GameEngine

#endif // PHYSICS_MANAGER_H
