#include "Physics/PhysicsManager.h"
#include "Platform/VitaMath.h"
#include "Components/Component.h"
#include "Components/PhysicsComponent.h"
#include "Components/Area3DComponent.h"
#include "Scene/SceneNode.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include <glm/gtc/quaternion.hpp>
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
#include <iostream>

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
            dynamicsWorld->removeCollisionObject(obj);
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

// 60 Hz.
const float PhysicsManager::FIXED_TIME_STEP = 1.0f / 60.0f;

void PhysicsManager::update(float deltaTime) {
    if (dynamicsWorld) {
        int maxSubSteps = 20;
        dynamicsWorld->stepSimulation(deltaTime, maxSubSteps, FIXED_TIME_STEP);
        syncAllTransformsFromPhysics();
    }
}

void PhysicsManager::stepSingleStep(float fixedTimeStep) {
    if (dynamicsWorld) {
        dynamicsWorld->stepSimulation(fixedTimeStep, 1, fixedTimeStep);
        syncAllTransformsFromPhysics();
    }
}

void PhysicsManager::syncAllTransformsFromPhysics() {
    for (auto* component : physicsComponents) {
        if (component && component->isEnabled()) {
            component->syncTransformFromPhysics();
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

void PhysicsManager::setDebugRay(const glm::vec3& from, const glm::vec3& to) {
    if (glm::length(to - from) > 1e-6f) {
        debugRays.push_back({ from, to });
    }
}

void PhysicsManager::renderDebugShapes(Material& debugMaterial,
                                      const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!debugDrawEnabled) return;
    
    for (auto* component : physicsComponents) {
        if (component && component->isEnabled() && component->getShowCollisionShape()) {
            component->renderDebugShape(debugMaterial, viewMatrix, projectionMatrix);
        }
    }
    for (const auto& ray : debugRays) {
        glm::vec3 wFrom = ray.from;
        glm::vec3 wTo = ray.to;
        glm::vec3 dir = wTo - wFrom;
        float L = glm::length(dir);
        if (L > 1e-6f) {
            glm::vec3 dirN = dir / L;
            glm::vec3 axis = glm::cross(glm::vec3(0.0f, 0.0f, -1.0f), dirN);
            float angle = acos(glm::clamp(-dirN.z, -1.0f, 1.0f));
            glm::mat4 R = glm::mat4(1.0f);
            if (glm::length(axis) > 1e-6f) {
                R = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));
            } else if (dirN.z > 0.99f) {
                R = glm::rotate(glm::mat4(1.0f), 3.14159265f, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, L));
            glm::mat4 T = glm::translate(glm::mat4(1.0f), wFrom);
            glm::mat4 modelMatrix = T * R * S;
            std::shared_ptr<Mesh> lineMesh = Mesh::createLineSegment();
            if (lineMesh) {
                lineMesh->bind();
                debugMaterial.apply();
                auto shader = debugMaterial.getShader();
                if (shader) {
                    shader->setMat4("modelMatrix", modelMatrix);
                    shader->setMat4("viewMatrix", viewMatrix);
                    shader->setMat4("projectionMatrix", projectionMatrix);
                }
                lineMesh->draw();
                lineMesh->unbind();
            }
        }
    }
    debugRays.clear();
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

// Raycast against all rigid bodies (static + dynamic).
struct AllCollidersRayResultCallback : public btCollisionWorld::ClosestRayResultCallback {
    std::string excludeNodeName;
    std::string excludeNodeName2;
    AllCollidersRayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld, const std::string& exclude, const std::string& exclude2 = std::string())
        : ClosestRayResultCallback(rayFromWorld, rayToWorld), excludeNodeName(exclude), excludeNodeName2(exclude2) {}
    virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
        const btCollisionObject* obj = rayResult.m_collisionObject;
        if (!btRigidBody::upcast(const_cast<btCollisionObject*>(obj)))
            return btScalar(2.0);
        if (obj->getUserPointer()) {
            PhysicsComponent* comp = static_cast<PhysicsComponent*>(obj->getUserPointer());
            if (comp && comp->getOwner()) {
                const std::string& name = comp->getOwner()->getName();
                if ((!excludeNodeName.empty() && name == excludeNodeName) ||
                    (!excludeNodeName2.empty() && name == excludeNodeName2))
                    return btScalar(2.0);
            }
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

// For excluding two nodes max, we could use a bitmask instead of two separate strings.
bool PhysicsManager::raycastGround(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                   const std::string& excludeNodeName, const std::string& excludeNodeName2,
                                   glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance) {
    hitPoint = origin;
    hitNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    hitDistance = maxDistance;
    if (!dynamicsWorld || maxDistance <= 0.0f) return false;
    glm::vec3 dir = direction;
    float len = glm::length(dir);
    if (len < 1e-6f) return false;
    dir /= len;
    btVector3 rayFrom(origin.x, origin.y, origin.z);
    btVector3 rayTo(origin.x + dir.x * maxDistance, origin.y + dir.y * maxDistance, origin.z + dir.z * maxDistance);
    AllCollidersRayResultCallback callback(rayFrom, rayTo, excludeNodeName, excludeNodeName2);
    callback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;
    callback.m_collisionFilterMask = btBroadphaseProxy::AllFilter;
    dynamicsWorld->rayTest(rayFrom, rayTo, callback);
    if (!callback.hasHit()) return false;
    hitPoint.x = callback.m_hitPointWorld.x();
    hitPoint.y = callback.m_hitPointWorld.y();
    hitPoint.z = callback.m_hitPointWorld.z();
    hitNormal.x = callback.m_hitNormalWorld.x();
    hitNormal.y = callback.m_hitNormalWorld.y();
    hitNormal.z = callback.m_hitNormalWorld.z();
    float nlen = glm::length(hitNormal);
    if (nlen > 1e-6f) hitNormal /= nlen;
    hitDistance = callback.m_closestHitFraction * maxDistance;
    return true;
}

struct MaskedRayResultCallback : public btCollisionWorld::ClosestRayResultCallback {
    MaskedRayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld)
        : ClosestRayResultCallback(rayFromWorld, rayToWorld), excludeNode(nullptr) {}
    MaskedRayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld, SceneNode* exclude)
        : ClosestRayResultCallback(rayFromWorld, rayToWorld), excludeNode(exclude) {}
    SceneNode* excludeNode;
    static bool isNodeUnder(SceneNode* node, SceneNode* ancestor) {
        if (!node || !ancestor) return false;
        for (SceneNode* n = node; n; n = n->getParent())
            if (n == ancestor) return true;
        return false;
    }
    virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
        if (excludeNode) {
            const btCollisionObject* obj = rayResult.m_collisionObject;
            if (obj && obj->getUserPointer()) {
                Component* comp = static_cast<Component*>(obj->getUserPointer());
                if (comp && comp->getOwner() && isNodeUnder(comp->getOwner(), excludeNode))
                    return btScalar(2.0);
            }
        }
        return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
    }
};

bool PhysicsManager::raycastFromTo(const glm::vec3& from, const glm::vec3& to, int collisionFilterMask,
                                   bool& outHit, std::string& hitNodeName, glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance) {
    return raycastFromTo(from, to, collisionFilterMask, outHit, hitNodeName, hitPoint, hitNormal, hitDistance, nullptr);
}

bool PhysicsManager::raycastFromTo(const glm::vec3& from, const glm::vec3& to, int collisionFilterMask,
                                   bool& outHit, std::string& hitNodeName, glm::vec3& hitPoint, glm::vec3& hitNormal, float& hitDistance,
                                   SceneNode* excludeNode) {
    outHit = false;
    hitNodeName.clear();
    hitPoint = from;
    hitNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    float maxDist = glm::length(to - from);
    if (!dynamicsWorld || maxDist < 1e-6f) return false;
    btVector3 rayFrom(from.x, from.y, from.z);
    btVector3 rayTo(to.x, to.y, to.z);
    btTransform rayFromTrans;
    rayFromTrans.setIdentity();
    rayFromTrans.setOrigin(rayFrom);
    btTransform rayToTrans;
    rayToTrans.setIdentity();
    rayToTrans.setOrigin(rayTo);
    short rayMask = (collisionFilterMask >= 0)
        ? static_cast<short>(collisionFilterMask)
        : static_cast<short>(btBroadphaseProxy::AllFilter | btBroadphaseProxy::SensorTrigger);

    // 1) Test sensors (ghosts) first with up-to-date world transform.
    MaskedRayResultCallback sensorCallback(rayFrom, rayTo, excludeNode);
    sensorCallback.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::SensorTrigger;
    sensorCallback.m_collisionFilterMask = rayMask;
    int n = dynamicsWorld->getNumCollisionObjects();
    for (int i = 0; i < n; ++i) {
        btCollisionObject* colObj = dynamicsWorld->getCollisionObjectArray()[i];
        btBroadphaseProxy* proxy = colObj->getBroadphaseHandle();
        if (!proxy || !(proxy->m_collisionFilterGroup & btBroadphaseProxy::SensorTrigger))
            continue;
        const btCollisionShape* shape = colObj->getCollisionShape();
        if (!shape) continue;
        btTransform ghostWorldTrans = colObj->getWorldTransform();
        void* userPtr = colObj->getUserPointer();
        if (userPtr) {
            Area3DComponent* areaComp = static_cast<Area3DComponent*>(userPtr);
            if (areaComp && areaComp->getOwner()) {
                glm::mat4 worldMat = areaComp->getOwner()->getWorldMatrix();
                glm::vec3 pos = glm::vec3(worldMat[3]);
                glm::mat3 rotMat = glm::mat3(worldMat);
                glm::quat q = glm::quat_cast(rotMat);
                ghostWorldTrans.setOrigin(btVector3(pos.x, pos.y, pos.z));
                ghostWorldTrans.setRotation(btQuaternion(q.x, q.y, q.z, q.w));
            }
        }
        btCollisionWorld::rayTestSingle(rayFromTrans, rayToTrans, colObj,
            shape, ghostWorldTrans, sensorCallback);
    }

    // 2) Test rigid bodies.
    MaskedRayResultCallback rigidCallback(rayFrom, rayTo, excludeNode);
    rigidCallback.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::SensorTrigger;
    rigidCallback.m_collisionFilterMask = rayMask;
    dynamicsWorld->rayTest(rayFrom, rayTo, rigidCallback);

    // 3) Take the closest hit.
    const MaskedRayResultCallback* callback = nullptr;
    if (sensorCallback.hasHit() && rigidCallback.hasHit()) {
        callback = (sensorCallback.m_closestHitFraction <= rigidCallback.m_closestHitFraction)
            ? &sensorCallback : &rigidCallback;
    } else if (sensorCallback.hasHit()) {
        callback = &sensorCallback;
    } else if (rigidCallback.hasHit()) {
        callback = &rigidCallback;
    }
    if (!callback || !callback->hasHit()) return false;
    outHit = true;
    hitPoint.x = callback->m_hitPointWorld.x();
    hitPoint.y = callback->m_hitPointWorld.y();
    hitPoint.z = callback->m_hitPointWorld.z();
    hitNormal.x = callback->m_hitNormalWorld.x();
    hitNormal.y = callback->m_hitNormalWorld.y();
    hitNormal.z = callback->m_hitNormalWorld.z();
    float nlen = glm::length(hitNormal);
    if (nlen > 1e-6f) hitNormal /= nlen;
    hitDistance = callback->m_closestHitFraction * maxDist;
    const btCollisionObject* obj = callback->m_collisionObject;
    if (obj && obj->getUserPointer()) {
        // Resolve node name for both PhysicsComponent and Area3DComponent.
        Component* comp = static_cast<Component*>(obj->getUserPointer());
        if (comp && comp->getOwner())
            hitNodeName = comp->getOwner()->getName();
    }
    return true;
}

} // namespace GameEngine


