#include "Physics/PhysicsManager.h"
#include "Platform/VitaMath.h"
#include "Components/PhysicsComponent.h"
#include "Scene/SceneNode.h"
#include "Core/ThreadManager.h"

// Bullet includes
#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <algorithm>
#include <iostream>

namespace GameEngine {


PhysicsManager::~PhysicsManager() {
    shutdown();
}

PhysicsManager& PhysicsManager::getInstance() {
    static PhysicsManager instance;
    return instance;
}

bool PhysicsManager::initialize() {
    collisionConfiguration = new btDefaultCollisionConfiguration();
    
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    
    btVector3 worldMin(-1000, -1000, -1000);
    btVector3 worldMax(1000, 1000, 1000);
    broadphase = new btAxisSweep3(worldMin, worldMax);
    
    solver = new btSequentialImpulseConstraintSolver();
    
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
    if (threadingEnabled && physicsThreadRunning) {
        PhysicsCommand shutdownCmd;
        shutdownCmd.type = PhysicsCommandType::SHUTDOWN;
        commandQueue.push(shutdownCmd);
        
        ThreadManager::getInstance().joinThread(physicsThread);
        physicsThreadRunning = false;
        commandQueue.reset();
        resultQueue.reset();
    }
    if (dynamicsWorld) {
        LockGuard lock(physicsMutex);
        
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
    
    physicsComponents.clear();
}

void PhysicsManager::update(float deltaTime) {
    if (!threadingEnabled) {
        // Single thread mode
        processPhysicsUpdate(deltaTime);
    } else {
        // Multi thread mode
        PhysicsCommand cmd;
        cmd.type = PhysicsCommandType::UPDATE;
        cmd.deltaTime = deltaTime;
        cmd.data = nullptr;
        commandQueue.push(cmd);
        syncPhysicsResults();
    }
}

void PhysicsManager::processPhysicsUpdate(float deltaTime) {
    if (!dynamicsWorld) {
        return;
    }
    
    LockGuard lock(physicsMutex);
    
    int maxSubSteps = 20;
    float fixedTimeStep = 1.0f / 60.0f;
    
    dynamicsWorld->stepSimulation(deltaTime, maxSubSteps, fixedTimeStep);
    
    if (threadingEnabled) {
        for (auto* component : physicsComponents) {
            if (component && component->isEnabled() && component->getRigidBody()) {
                btRigidBody* body = component->getRigidBody();
                if (body && body->getMotionState()) {
                    btTransform transform;
                    body->getMotionState()->getWorldTransform(transform);
                    
                    PhysicsTransformResult result;
                    result.component = component;
                    result.position = glm::vec3(
                        transform.getOrigin().x(),
                        transform.getOrigin().y(),
                        transform.getOrigin().z()
                    );
                    btQuaternion rot = transform.getRotation();
                    result.rotation = glm::quat(rot.w(), rot.x(), rot.y(), rot.z());
                    result.valid = true;
                    resultQueue.push(result);
                }
            }
        }
    }
}

void PhysicsManager::syncPhysicsResults() {
    if (!threadingEnabled) {
        for (auto* component : physicsComponents) {
            if (component && component->isEnabled()) {
                component->syncTransformFromPhysics();
            }
        }
        return;
    }
    
    PhysicsTransformResult result;
    while (resultQueue.tryPop(result)) {
        if (result.valid && result.component) {
            applyTransformToComponent(result.component, result.position, result.rotation);
        }
    }
}

void PhysicsManager::syncComponentTransformFromPhysics(PhysicsComponent* component) {
    if (!component) {
        return;
    }
    
    if (threadingEnabled) {
        LockGuard lock(physicsMutex);
        component->syncTransformFromPhysics();
    } else {
        component->syncTransformFromPhysics();
    }
}

void PhysicsManager::applyTransformToComponent(PhysicsComponent* component, const glm::vec3& position, const glm::quat& rotation) {
    if (!component) {
        return;
    }
    
    auto* owner = component->getOwner();
    if (!owner) {
        return;
    }
    
    auto bodyType = component->getBodyType();
    
    if (bodyType == PhysicsBodyType::DYNAMIC || bodyType == PhysicsBodyType::KINEMATIC) {
        bool rotationLocked = false;
        if (component->getRigidBody()) {
            btVector3 angularFactor = component->getRigidBody()->getAngularFactor();
            rotationLocked = (angularFactor.x() == 0.0f && angularFactor.y() == 0.0f && angularFactor.z() == 0.0f);
        }
        
        if (owner->getParent()) {
            if (bodyType == PhysicsBodyType::DYNAMIC) {
                auto& parentTransform = owner->getParent()->getTransform();
                parentTransform.setPosition(position);
                if (!rotationLocked) {
                    parentTransform.setRotation(rotation);
                }
            }
        } else {
            owner->getTransform().setPosition(position);
            if (!rotationLocked) {
                owner->getTransform().setRotation(rotation);
            }
        }
    }
}

void PhysicsManager::physicsThreadFunction() {
    physicsThreadRunning = true;
    
    while (physicsThreadRunning) {
        PhysicsCommand cmd;
        if (commandQueue.pop(cmd)) {
            switch (cmd.type) {
                case PhysicsCommandType::UPDATE:
                    processPhysicsUpdate(cmd.deltaTime);
                    break;
                    
                case PhysicsCommandType::ADD_RIGID_BODY:
                    if (cmd.data) {
                        LockGuard lock(physicsMutex);
                        addRigidBodyInternal(static_cast<btRigidBody*>(cmd.data));
                    }
                    break;
                    
                case PhysicsCommandType::REMOVE_RIGID_BODY:
                    if (cmd.data) {
                        LockGuard lock(physicsMutex);
                        removeRigidBodyInternal(static_cast<btRigidBody*>(cmd.data));
                    }
                    break;
                    
                case PhysicsCommandType::SET_GRAVITY:
                    if (cmd.data) {
                        LockGuard lock(physicsMutex);
                        glm::vec3* gravity = static_cast<glm::vec3*>(cmd.data);
                        setGravityInternal(*gravity);
                        delete gravity;
                    }
                    break;
                    
                case PhysicsCommandType::SHUTDOWN:
                    physicsThreadRunning = false;
                    break;
            }
        } else {
            physicsThreadRunning = false;
        }
    }
}

void PhysicsManager::enableThreading(bool enable) {
    if (threadingEnabled == enable) {
        return;
    }
    
    threadingEnabled = enable;
    
    if (enable && !physicsThreadRunning) {
        physicsThread = ThreadManager::getInstance().createThread(
            "PhysicsThread",
            [this]() { this->physicsThreadFunction(); }
        );
        
        if (!ThreadManager::getInstance().isValid(physicsThread)) {
            std::cerr << "PhysicsManager: Failed to create physics thread!" << std::endl;
            threadingEnabled = false;
        } else {
            std::cout << "PhysicsManager: Physics threading enabled successfully" << std::endl;
        }
    } else if (!enable) {
        if (physicsThreadRunning) {
            PhysicsCommand shutdownCmd;
            shutdownCmd.type = PhysicsCommandType::SHUTDOWN;
            commandQueue.push(shutdownCmd);
            
            ThreadManager::getInstance().joinThread(physicsThread);
            physicsThreadRunning = false;
            commandQueue.reset();
            resultQueue.reset();
            
            std::cout << "PhysicsManager: Physics threading disabled (Total threads: " 
                      << ThreadManager::getInstance().getThreadCount() << ")" << std::endl;
        } else {
            std::cout << "PhysicsManager: Physics threading disabled (was already disabled)" << std::endl;
        }
    }
}

void PhysicsManager::addRigidBody(btRigidBody* body) {
    if (!dynamicsWorld || !body) {
        return;
    }
    
    if (threadingEnabled) {
        PhysicsCommand cmd;
        cmd.type = PhysicsCommandType::ADD_RIGID_BODY;
        cmd.data = body;
        commandQueue.push(cmd);
    } else {
        LockGuard lock(physicsMutex);
        dynamicsWorld->addRigidBody(body);
    }
}

void PhysicsManager::removeRigidBody(btRigidBody* body) {
    if (!dynamicsWorld || !body) {
        return;
    }
    
    if (threadingEnabled) {
        PhysicsCommand cmd;
        cmd.type = PhysicsCommandType::REMOVE_RIGID_BODY;
        cmd.data = body;
        commandQueue.push(cmd);
    } else {
        LockGuard lock(physicsMutex);
        dynamicsWorld->removeRigidBody(body);
    }
}

void PhysicsManager::setGravity(const glm::vec3& gravity) {
    if (!dynamicsWorld) {
        return;
    }
    
    if (threadingEnabled) {
        PhysicsCommand cmd;
        cmd.type = PhysicsCommandType::SET_GRAVITY;
        cmd.data = new glm::vec3(gravity);
        commandQueue.push(cmd);
    } else {
        LockGuard lock(physicsMutex);
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
    // TODO: Implement debug drawer
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

void PhysicsManager::addRigidBodyInternal(btRigidBody* body) {
    if (!dynamicsWorld || !body) {
        return;
    }
    dynamicsWorld->addRigidBody(body);
}

void PhysicsManager::removeRigidBodyInternal(btRigidBody* body) {
    if (!dynamicsWorld || !body) {
        return;
    }
    dynamicsWorld->removeRigidBody(body);
}

void PhysicsManager::setGravityInternal(const glm::vec3& gravity) {
    if (!dynamicsWorld) {
        return;
    }
    dynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
}

void PhysicsManager::cleanupPhysicsObjects() {
}

} // namespace GameEngine


