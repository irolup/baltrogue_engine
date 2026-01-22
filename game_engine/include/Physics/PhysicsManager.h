#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Core/ThreadManager.h"

class btDiscreteDynamicsWorld;
class btCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btConstraintSolver;
class btRigidBody;
class btCollisionShape;
class btMotionState;
class btGhostPairCallback;

// Forward declarations for rendering
namespace GameEngine {
    class Renderer;
    class Material;
}

namespace GameEngine {

class PhysicsComponent;

enum class PhysicsCommandType {
    UPDATE,
    ADD_RIGID_BODY,
    REMOVE_RIGID_BODY,
    SET_GRAVITY,
    SHUTDOWN
};

struct PhysicsCommand {
    PhysicsCommandType type;
    float deltaTime;
    void* data;
};

struct PhysicsTransformResult {
    PhysicsComponent* component;
    glm::vec3 position;
    glm::quat rotation;
    bool valid;
};

class PhysicsManager {
public:
    static PhysicsManager& getInstance();
    
    // Physics system lifecycle
    bool initialize();
    void shutdown();
    void update(float deltaTime);
    
    void enableThreading(bool enable);
    bool isThreadingEnabled() const { return threadingEnabled; }
    
    // Sync physics results from physics thread to main thread
    void syncPhysicsResults();
    
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
    
    void syncComponentTransformFromPhysics(PhysicsComponent* component);
    void applyTransformToComponent(PhysicsComponent* component, const glm::vec3& position, const glm::quat& rotation);
    
private:
    PhysicsManager() 
        : debugDrawEnabled(false), threadingEnabled(false)
        , commandQueue("PhysicsCommandQueue"), resultQueue("PhysicsResultQueue")
        #ifndef LINUX_BUILD
        , physicsMutex("PhysicsMutex")
        #endif
        , physicsThreadRunning(false) {}
    ~PhysicsManager();
    
    // Bullet physics objects
    btDiscreteDynamicsWorld* dynamicsWorld;
    btCollisionConfiguration* collisionConfiguration;
    btCollisionDispatcher* dispatcher;
    btBroadphaseInterface* broadphase;
    btConstraintSolver* solver;
    btGhostPairCallback* ghostPairCallback;
    
    // Physics components
    std::vector<PhysicsComponent*> physicsComponents;
    
    // Debug drawing
    bool debugDrawEnabled;
    
    // Threading
    bool threadingEnabled;
    ThreadHandle physicsThread;
    ThreadSafeQueue<PhysicsCommand> commandQueue;
    ThreadSafeQueue<PhysicsTransformResult> resultQueue;
    Mutex physicsMutex;
    std::atomic<bool> physicsThreadRunning;

    // Private helper methods
    void processPhysicsUpdate(float deltaTime);
    void physicsThreadFunction();
    void cleanupPhysicsObjects();
    
    // Internal methods for direct physics world access (used by physics thread)
    void addRigidBodyInternal(btRigidBody* body);
    void removeRigidBodyInternal(btRigidBody* body);
    void setGravityInternal(const glm::vec3& gravity);
};

} // namespace GameEngine

#endif // PHYSICS_MANAGER_H
