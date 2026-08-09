#ifdef LINUX_BUILD

#include "Core/Engine.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/PhysicsComponent.h"
#include "Components/TextComponent.h"
#include "Components/LightComponent.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/TextureManager.h"
#include "Physics/PhysicsManager.h"
#include <iostream>

using namespace GameEngine;

int main() {
    Engine engine;
    
    if (!engine.initialize(EngineMode::EDITOR)) {
        std::cerr << "Failed to initialize game engine in editor mode!" << std::endl;
        return -1;
    }
    
    engine.setWindowTitle("Game Engine Editor");
    
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
    
    auto& sceneManager = engine.getSceneManager();
    auto testScene = sceneManager.createScene("Test Scene");
    
    auto cameraNode = testScene->createNode("Main Camera");
    cameraNode->addComponent<CameraComponent>();
    cameraNode->getTransform().setPosition(glm::vec3(0, 0, 5));
    cameraNode->getTransform().setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    testScene->getRootNode()->addChild(cameraNode);
    testScene->setActiveCamera(cameraNode);

    // Default light so the lit cube is visible
    auto lightNode = testScene->createNode("Default Light");
    auto lightComponent = lightNode->addComponent<LightComponent>();
    lightComponent->setType(LightType::DIRECTIONAL);
    lightComponent->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
    lightComponent->setIntensity(1.0f);
    lightComponent->setShowGizmo(true);
    lightNode->getTransform().setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    lightNode->getTransform().setEulerAngles(glm::vec3(-45.0f, 0.0f, 0.0f));
    testScene->getRootNode()->addChild(lightNode);
    lightComponent->start();
    
    auto cubeNode = testScene->createNode("Test Cube");
    auto meshRenderer = cubeNode->addComponent<MeshRenderer>();
    meshRenderer->setMesh(Mesh::createCube());
    
    auto material = std::make_shared<GameEngine::Material>();
    material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
    
    
    meshRenderer->setMaterial(material);
    
    cubeNode->getTransform().setPosition(glm::vec3(0, 0, 0));
    cubeNode->getTransform().setScale(glm::vec3(1.0f, 1.0f, 1.0f));
    testScene->getRootNode()->addChild(cubeNode);

    
    // Load the test scene
    sceneManager.loadScene(testScene);
    
    // Set the scene in the editor
    engine.getEditor().setActiveScene(testScene);
    
    // Run the editor
    engine.run();
    
    return 0;
}

#endif
