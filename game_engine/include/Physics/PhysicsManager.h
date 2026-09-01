#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#include <memory>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <functional>
#include <glm/glm.hpp>

class btDiscreteDynamicsWorld;
class btCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btConstraintSolver;
class btRigidBody;
class btCollisionObject;
class btTypedConstraint;
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
class JointComponent;


class PhysicsManager {
public:
    static PhysicsManager& getInstance();
    
    // Fixed timestep (60 Hz), same idea as Unity FixedUpdate / Godot _physics_process rate
    static const float FIXED_TIME_STEP;

    // Physics system lifecycle
    bool initialize();
    void shutdown();
    void update(float deltaTime);
    /** Advance physics by one fixed step (for fixed-rate loop). Use with fixedUpdate() in scripts. */
    void stepSingleStep(float fixedTimeStep);
    
    
    // Physics world management
    btDiscreteDynamicsWorld* getDynamicsWorld() const { return dynamicsWorld; }
    
    void addRigidBody(btRigidBody* body);
    void addRigidBody(btRigidBody* body, int collisionFilterGroup, int collisionFilterMask);
    void removeRigidBody(btRigidBody* body);

    void addConstraint(btTypedConstraint* constraint, bool disableCollisionsBetweenLinkedBodies = true);
    void removeConstraint(btTypedConstraint* constraint);

    void addSensorObject(btCollisionObject* object);
    void removeSensorObject(btCollisionObject* object);
    
    // Physics properties
    void setGravity(const glm::vec3& gravity);
    glm::vec3 getGravity() const;
    
    // Collision shape creation helpers
    btCollisionShape* createBoxShape(const glm::vec3& halfExtents);
    btCollisionShape* createSphereShape(float radius);
    btCollisionShape* createCapsuleShape(float radius, float height);
    btCollisionShape* createCylinderShape(const glm::vec3& halfExtents);
    btCollisionShape* createRampShape(const glm::vec3& halfExtents);
    btCollisionShape* createPlaneShape(const glm::vec3& normal, float constant);
    
    // Debug drawing
    void setDebugDrawEnabled(bool enabled);
    bool isDebugDrawEnabled() const;
    /** Add a world-space ray segment to draw when debug draw is enabled. Call from Lua (setDebugRayFromTo) for each ray; all are drawn. */
    void setDebugRay(const glm::vec3& from, const glm::vec3& to);
    
    // Debug rendering
    void renderDebugShapes(Material& debugMaterial, 
                          const glm::mat4& viewMatrix = glm::mat4(1.0f), 
                          const glm::mat4& projectionMatrix = glm::mat4(1.0f));
    
    // Physics component registration
    void registerPhysicsComponent(PhysicsComponent* component);
    void unregisterPhysicsComponent(PhysicsComponent* component);

    void registerJointComponent(JointComponent* component);
    void unregisterJointComponent(JointComponent* component);
    // Release any joint constraints referencing this body before it is deleted
    void notifyRigidBodyDestroyed(btRigidBody* body);
    
    void raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                 std::string& hitNodeName, glm::vec3& hitPoint, float& hitDistance);

    float raycastClosestObstacle(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                 const std::string& excludeNodeName = std::string());

    bool raycastGround(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                       const std::string& excludeNodeName, const std::string& excludeNodeName2,
                       glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance,
                       std::string* outHitNodeName = nullptr,
                       float* outSurfaceFriction = nullptr);

    bool raycastFromTo(const glm::vec3& from, const glm::vec3& to, int collisionFilterMask,
                       bool& hit, std::string& hitNodeName, glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance,
                       float* outSurfaceFriction = nullptr);
    bool raycastFromTo(const glm::vec3& from, const glm::vec3& to, int collisionFilterMask,
                       bool& hit, std::string& hitNodeName, glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance,
                       class SceneNode* excludeNode, float* outSurfaceFriction = nullptr);

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
    std::vector<JointComponent*> jointComponents;
    std::vector<btCollisionObject*> sensorObjects;
    
    // Debug drawing
    bool debugDrawEnabled;
    struct DebugRaySegment {
        glm::vec3 from;
        glm::vec3 to;
    };
    std::vector<DebugRaySegment> debugRays;

    void cleanupPhysicsObjects();
    void syncAllTransformsFromPhysics();

    void dispatchContactEvents();

    using ContactPair = std::pair<PhysicsComponent*, PhysicsComponent*>;
    struct ContactPairHash {
        size_t operator()(const ContactPair& pair) const {
            const size_t a = std::hash<const void*>()(pair.first);
            const size_t b = std::hash<const void*>()(pair.second);
            return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
        }
    };
    std::unordered_set<ContactPair, ContactPairHash> previousContactPairs_;
    std::unordered_set<ContactPair, ContactPairHash> currentContactPairs_;
    std::unordered_map<const btCollisionObject*, PhysicsComponent*> contactBodyLookup_;
};

} // namespace GameEngine

#endif // PHYSICS_MANAGER_H
