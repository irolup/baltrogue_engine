#include "Physics/PhysicsManager.h"
#include "Platform/VitaMath.h"
#include "Components/PhysicsComponent.h"
#include "Scene/SceneNode.h"
#include "Core/ThreadManager.h"

// Bullet includes
#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletCollision/BroadphaseCollision/btBroadphaseProxy.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h"
#include "BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h"
#include "LinearMath/btThreads.h"
#include <algorithm>
#include <thread>

#ifdef VITA_BUILD
extern "C" void btSetDesiredVitaThreadCount(int count);
#endif

namespace GameEngine {

#ifdef VITA_BUILD
static const bool ENABLE_PHYSICS_MULTITHREADING = true;
static const int MAX_PHYSICS_THREADS_VITA = 1; // Max number on vita is 2 worker threads, 0 = physics on main thread only
#else
static const bool ENABLE_PHYSICS_MULTITHREADING = true;
static const int MAX_PHYSICS_THREADS = 8;
#endif

PhysicsManager::PhysicsManager()
    : dynamicsWorld(nullptr)
    , collisionConfiguration(nullptr)
    , dispatcher(nullptr)
    , broadphase(nullptr)
    , solver(nullptr)
    , ghostPairCallback(nullptr)
    , debugDrawEnabled(false)
{   
}


PhysicsManager::~PhysicsManager() {
    shutdown();
}

PhysicsManager& PhysicsManager::getInstance() {
    static PhysicsManager instance;
    return instance;
}

bool PhysicsManager::initialize() {
    collisionConfiguration = new btDefaultCollisionConfiguration();
    
    scheduler = nullptr;
    bool useMultithreading = false;
    
    #if BT_THREADSAFE
    if (ENABLE_PHYSICS_MULTITHREADING) {
        #ifdef VITA_BUILD
        btSetDesiredVitaThreadCount(MAX_PHYSICS_THREADS_VITA);
        #endif
        
        scheduler = btGetTaskScheduler();
        if (!scheduler) {
            scheduler = btCreateDefaultTaskScheduler();
            if (scheduler) {
                btSetTaskScheduler(scheduler);
            } else {
                printf("[PhysicsManager] WARNING: Failed to create task scheduler\n");
            }
        }
        
        if (scheduler) {
            int requestedThreads;
            #ifdef VITA_BUILD
            requestedThreads = MAX_PHYSICS_THREADS_VITA + 1;
            #else
            int availableCores = std::thread::hardware_concurrency();
            requestedThreads = std::max(1, std::min(availableCores - 1, MAX_PHYSICS_THREADS));
            #endif
            
            scheduler->setNumThreads(requestedThreads);
            int actualThreads = scheduler->getNumThreads();
            useMultithreading = (actualThreads > 1);
            
            printf("[PhysicsManager] Physics initialized: %d thread(s), multithreading %s\n", 
                   actualThreads, useMultithreading ? "ENABLED" : "DISABLED");
        }
    }
    #endif
    
    #if BT_THREADSAFE
    if (ENABLE_PHYSICS_MULTITHREADING && useMultithreading && scheduler) {
        btITaskScheduler* activeScheduler = btGetTaskScheduler();
        if (activeScheduler && activeScheduler->getNumThreads() > 1) {
            dispatcher = new btCollisionDispatcherMt(collisionConfiguration);
            solver = new btSequentialImpulseConstraintSolverMt();
        } else {
            dispatcher = new btCollisionDispatcher(collisionConfiguration);
            solver = new btSequentialImpulseConstraintSolver();
        }
    } else {
        dispatcher = new btCollisionDispatcher(collisionConfiguration);
        solver = new btSequentialImpulseConstraintSolver();
    }
    #else
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    solver = new btSequentialImpulseConstraintSolver();
    #endif
    
    btVector3 worldMin(-1000, -1000, -1000);
    btVector3 worldMax(1000, 1000, 1000);
    broadphase = new btAxisSweep3(worldMin, worldMax);
    //broadphase = new btDbvtBroadphase(); if we want to use the dbvt broadphase and remove the axis sweep 3 and the min and max vectors
    
    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    
    ghostPairCallback = new btGhostPairCallback();
    dynamicsWorld->getPairCache()->setInternalGhostPairCallback(ghostPairCallback);
    
    dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));
    
    dynamicsWorld->getDispatchInfo().m_allowedCcdPenetration = 0.0001f;
    dynamicsWorld->getDispatchInfo().m_useContinuous = true;
    
    dynamicsWorld->getSolverInfo().m_solverMode |= SOLVER_ENABLE_FRICTION_DIRECTION_CACHING;
    dynamicsWorld->getSolverInfo().m_solverMode |= SOLVER_RANDMIZE_ORDER;
    dynamicsWorld->getSolverInfo().m_numIterations = 10;
    
    return true;
}

void PhysicsManager::shutdown() {
    if (dynamicsWorld) {
        for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMotionState()) {
                delete body->getMotionState();
            }
            dynamicsWorld->removeCollisionObject(obj);
            delete obj;
        }
        
        delete dynamicsWorld;
        dynamicsWorld = nullptr;
    }
    
    if (solver) {
        delete solver;
        solver = nullptr;
    }
    
    if (broadphase) {
        delete broadphase;
        broadphase = nullptr;
    }
    
    if (dispatcher) {
        delete dispatcher;
        dispatcher = nullptr;
    }
    
    if (collisionConfiguration) {
        delete collisionConfiguration;
        collisionConfiguration = nullptr;
    }
    
    if (ghostPairCallback) {
        delete ghostPairCallback;
        ghostPairCallback = nullptr;
    }

    if (scheduler) {
        delete scheduler;
        scheduler = nullptr;
    }
    
    physicsComponents.clear();
}

void PhysicsManager::update(float deltaTime) {
    if (dynamicsWorld) {
        int maxSubSteps = 20;
        float fixedTimeStep = 1.0f / 60.0f;
        
        dynamicsWorld->stepSimulation(deltaTime, maxSubSteps, fixedTimeStep);
        
        for (auto* component : physicsComponents) {
            if (component && component->isEnabled()) {
                component->syncTransformFromPhysics();
            }
        }
    }
}

void PhysicsManager::addRigidBody(btRigidBody* body) {
    if (dynamicsWorld && body) {
        dynamicsWorld->addRigidBody(body);
    }
}

void PhysicsManager::addRigidBody(btRigidBody* body, int collisionFilterGroup, int collisionFilterMask) {
    if (dynamicsWorld && body) {
        dynamicsWorld->addRigidBody(body, collisionFilterGroup, collisionFilterMask);
    }
}

void PhysicsManager::removeRigidBody(btRigidBody* body) {
    if (dynamicsWorld && body) {
        dynamicsWorld->removeRigidBody(body);
    }
}

void PhysicsManager::addConstraint(btTypedConstraint* constraint, bool disableCollisionsBetweenLinkedBodies) {
    if (dynamicsWorld && constraint) {
        dynamicsWorld->addConstraint(constraint, disableCollisionsBetweenLinkedBodies);
    }
}

void PhysicsManager::removeConstraint(btTypedConstraint* constraint) {
    if (dynamicsWorld && constraint) {
        dynamicsWorld->removeConstraint(constraint);
    }
}

void PhysicsManager::setGravity(const glm::vec3& gravity) {
    if (dynamicsWorld) {
        dynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
    }
}

glm::vec3 PhysicsManager::getGravity() const {
    if (dynamicsWorld) {
        btVector3 gravity = dynamicsWorld->getGravity();
        return glm::vec3(gravity.x(), gravity.y(), gravity.z());
    }
    return glm::vec3(0, -9.81f, 0);
}

btCollisionShape* PhysicsManager::createBoxShape(const glm::vec3& halfExtents) {
    return new btBoxShape(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
}

btCollisionShape* PhysicsManager::createSphereShape(float radius) {
    return new btSphereShape(radius);
}

btCollisionShape* PhysicsManager::createCapsuleShape(float radius, float height) {
    return new btCapsuleShape(radius, height);
}

btCollisionShape* PhysicsManager::createCylinderShape(const glm::vec3& halfExtents) {
    return new btCylinderShape(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
}

btCollisionShape* PhysicsManager::createPlaneShape(const glm::vec3& normal, float constant) {
    return new btStaticPlaneShape(btVector3(normal.x, normal.y, normal.z), constant);
}

void PhysicsManager::setDebugDrawEnabled(bool enabled) {
    debugDrawEnabled = enabled;
}

bool PhysicsManager::isDebugDrawEnabled() const {
    return debugDrawEnabled;
}

void PhysicsManager::renderDebugShapes(Material& debugMaterial,
                                      const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!debugDrawEnabled) return;
    
    for (auto* component : physicsComponents) {
        if (component && component->isEnabled() && component->getShowCollisionShape()) {
            component->renderDebugShape(debugMaterial, viewMatrix, projectionMatrix);
        }
    }
}

void PhysicsManager::registerPhysicsComponent(PhysicsComponent* component) {
    if (component) {
        auto it = std::find(physicsComponents.begin(), physicsComponents.end(), component);
        if (it == physicsComponents.end()) {
            physicsComponents.push_back(component);
        }
    }
}

void PhysicsManager::unregisterPhysicsComponent(PhysicsComponent* component) {
    if (component) {
        auto it = std::find(physicsComponents.begin(), physicsComponents.end(), component);
        if (it != physicsComponents.end()) {
            physicsComponents.erase(it);
        }
    }
}

void PhysicsManager::cleanupPhysicsObjects() {
}

// Raycast against all rigid bodies (static + dynamic) to find obstacle distance; optionally exclude one node (e.g. held object). Ignores ghost/trigger objects.
struct AllCollidersRayResultCallback : public btCollisionWorld::ClosestRayResultCallback {
    std::string excludeNodeName;
    AllCollidersRayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld, const std::string& exclude)
        : ClosestRayResultCallback(rayFromWorld, rayToWorld), excludeNodeName(exclude) {}
    virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
        const btCollisionObject* obj = rayResult.m_collisionObject;
        if (!btRigidBody::upcast(const_cast<btCollisionObject*>(obj)))
            return btScalar(2.0);
        if (obj->getUserPointer() && !excludeNodeName.empty()) {
            PhysicsComponent* comp = static_cast<PhysicsComponent*>(obj->getUserPointer());
            if (comp && comp->getOwner() && comp->getOwner()->getName() == excludeNodeName)
                return btScalar(2.0);
        }
        return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
    }
};

void PhysicsManager::raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                             std::string& hitNodeName, glm::vec3& hitPoint, float& hitDistance) {
    hitNodeName.clear();
    hitPoint = origin;
    hitDistance = maxDistance;
    if (!dynamicsWorld || maxDistance <= 0.0f) return;
    glm::vec3 dir = direction;
    float len = glm::length(dir);
    if (len < 1e-6f) return;
    dir /= len;
    btVector3 rayFrom(origin.x, origin.y, origin.z);
    btVector3 rayTo(origin.x + dir.x * maxDistance, origin.y + dir.y * maxDistance, origin.z + dir.z * maxDistance);
    AllCollidersRayResultCallback callback(rayFrom, rayTo, "");
    callback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;
    callback.m_collisionFilterMask = btBroadphaseProxy::AllFilter;
    dynamicsWorld->rayTest(rayFrom, rayTo, callback);
    if (!callback.hasHit()) return;
    const btCollisionObject* obj = callback.m_collisionObject;
    if (!obj || !obj->getUserPointer()) return;
    btRigidBody* body = btRigidBody::upcast(const_cast<btCollisionObject*>(obj));
    if (!body || body->isKinematicObject() || body->isStaticObject()) return;
    PhysicsComponent* comp = static_cast<PhysicsComponent*>(obj->getUserPointer());
    if (!comp || !comp->getOwner() || comp->getBodyType() != PhysicsBodyType::DYNAMIC) return;
    hitNodeName = comp->getOwner()->getName();
    hitPoint.x = callback.m_hitPointWorld.x();
    hitPoint.y = callback.m_hitPointWorld.y();
    hitPoint.z = callback.m_hitPointWorld.z();
    hitDistance = callback.m_closestHitFraction * maxDistance;
}

float PhysicsManager::raycastClosestObstacle(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                             const std::string& excludeNodeName) {
    if (!dynamicsWorld || maxDistance <= 0.0f) return -1.0f;
    glm::vec3 dir = direction;
    float len = glm::length(dir);
    if (len < 1e-6f) return -1.0f;
    dir /= len;
    btVector3 rayFrom(origin.x, origin.y, origin.z);
    btVector3 rayTo(origin.x + dir.x * maxDistance, origin.y + dir.y * maxDistance, origin.z + dir.z * maxDistance);
    AllCollidersRayResultCallback callback(rayFrom, rayTo, excludeNodeName);
    callback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;
    callback.m_collisionFilterMask = btBroadphaseProxy::AllFilter;
    dynamicsWorld->rayTest(rayFrom, rayTo, callback);
    if (!callback.hasHit()) return -1.0f;
    return callback.m_closestHitFraction * maxDistance;
}

} // namespace GameEngine


