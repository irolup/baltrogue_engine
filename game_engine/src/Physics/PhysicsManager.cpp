#include "Physics/PhysicsManager.h"
#include "Platform/VitaMath.h"
#include "Components/PhysicsComponent.h"

// Bullet includes
#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>
#include <algorithm>

namespace GameEngine {

PhysicsManager::PhysicsManager()
    : dynamicsWorld(nullptr)
    , collisionConfiguration(nullptr)
    , dispatcher(nullptr)
    , broadphase(nullptr)
    , solver(nullptr)
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
    
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    
    btVector3 worldMin(-1000, -1000, -1000);
    btVector3 worldMax(1000, 1000, 1000);
    broadphase = new btAxisSweep3(worldMin, worldMax);
    
    solver = new btSequentialImpulseConstraintSolver();
    
    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    
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

void PhysicsManager::removeRigidBody(btRigidBody* body) {
    if (dynamicsWorld && body) {
        dynamicsWorld->removeRigidBody(body);
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

void PhysicsManager::cleanupPhysicsObjects() {
}

} // namespace GameEngine
