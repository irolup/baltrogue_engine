#ifdef LINUX_BUILD

#include "Core/Engine.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/PhysicsComponent.h"
#include "Components/TextComponent.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/TextureManager.h"
#include "Physics/PhysicsManager.h"
#include <iostream>

using namespace GameEngine;

int main() {
    // Create engine instance
    Engine engine;
    
    // Initialize in editor mode
    if (!engine.initialize(EngineMode::EDITOR)) {
        std::cerr << "Failed to initialize game engine in editor mode!" << std::endl;
        return -1;
    }
    
    engine.setWindowTitle("Game Engine Editor");
    
    // Initialize texture discovery
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
    
    // Create a test scene
    auto& sceneManager = engine.getSceneManager();
    auto testScene = sceneManager.createScene("Test Scene");
    
    // Create a camera first!
    auto cameraNode = testScene->createNode("Main Camera");
    cameraNode->addComponent<CameraComponent>();
    cameraNode->getTransform().setPosition(glm::vec3(0, 0, 5));
    cameraNode->getTransform().setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)); // Identity rotation (looking down -Z)
    testScene->getRootNode()->addChild(cameraNode);
    testScene->setActiveCamera(cameraNode);
    
    // Add some test objects
    auto cubeNode = testScene->createNode("Test Cube");
    auto meshRenderer = cubeNode->addComponent<MeshRenderer>();
    meshRenderer->setMesh(Mesh::createCube());
    
    // Create a unique material instance for the test cube
    auto material = std::make_shared<GameEngine::Material>();
    material->setColor(glm::vec3(1.0f, 0.5f, 0.2f)); // Default orange color
    
    // Try to load red brick textures
    auto diffuseTexture = textureManager.getTexture("assets/textures/red_brick/red_brick_diff_1k.png");
    auto normalTexture = textureManager.getTexture("assets/textures/red_brick/red_brick_nor_gl_1k.png");
    auto armTexture = textureManager.getTexture("assets/textures/red_brick/red_brick_arm_1k.png");
    
    if (diffuseTexture) {
        material->setDiffuseTexture(diffuseTexture, "assets/textures/red_brick/red_brick_diff_1k.png");
        std::cout << "Loaded diffuse texture" << std::endl;
    }
    if (normalTexture) {
        material->setNormalTexture(normalTexture, "assets/textures/red_brick/red_brick_nor_gl_1k.png");
        std::cout << "Loaded normal texture" << std::endl;
    }
    if (armTexture) {
        material->setARMTexture(armTexture, "assets/textures/red_brick/red_brick_arm_1k.png");
        std::cout << "Loaded ARM texture" << std::endl;
    }
    
    meshRenderer->setMaterial(material);
    
    cubeNode->getTransform().setPosition(glm::vec3(0, 0, 0));
    cubeNode->getTransform().setScale(glm::vec3(1.0f, 1.0f, 1.0f));
    testScene->getRootNode()->addChild(cubeNode);
    
    // Add a test model (if available)
    auto modelNode = testScene->createNode("Test Model");
    auto modelRenderer = modelNode->addComponent<ModelRenderer>();
    
    // Try to load the lemon model
    if (modelRenderer->loadModel("assets/models/lemon_1k.gltf/lemon_1k.gltf")) {
        std::cout << "Successfully loaded lemon model!" << std::endl;
        modelNode->getTransform().setPosition(glm::vec3(5, 0, 0));
        modelNode->getTransform().setScale(glm::vec3(5.0f, 5.0f, 5.0f));
        testScene->getRootNode()->addChild(modelNode);
    } else {
        std::cout << "Failed to load lemon model, continuing without it" << std::endl;
    }
    
    // Add test text components
    std::cout << "Creating test text components..." << std::endl;
    
    // World space text
    auto worldTextNode = testScene->createNode("World Text");
    auto worldTextComponent = worldTextNode->addComponent<TextComponent>();
    worldTextComponent->setText("Hello World!");
    worldTextComponent->setFontPath("assets/fonts/DroidSans.ttf");
    worldTextComponent->setFontSize(32.0f);
    worldTextComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    worldTextComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
    worldTextComponent->setAlignment(TextAlignment::CENTER);
    worldTextNode->getTransform().setPosition(glm::vec3(0, 2, 0));
    testScene->getRootNode()->addChild(worldTextNode);
    
    // Screen space text (child of camera)
    auto screenTextNode = testScene->createNode("Screen Text");
    auto screenTextComponent = screenTextNode->addComponent<TextComponent>();
    screenTextComponent->setText("Screen Space Text");
    screenTextComponent->setFontPath("assets/fonts/DroidSans.ttf");
    screenTextComponent->setFontSize(24.0f);
    screenTextComponent->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    screenTextComponent->setRenderMode(TextRenderMode::SCREEN_SPACE);
    screenTextComponent->setAlignment(TextAlignment::LEFT);
    screenTextNode->getTransform().setPosition(glm::vec3(-2, 1, 0));
    cameraNode->addChild(screenTextNode);
    
    std::cout << "Test text components created!" << std::endl;
    
    // Load the test scene
    sceneManager.loadScene(testScene);
    
    // Set the scene in the editor
    engine.getEditor().setActiveScene(testScene);
    
    // Run the editor
    engine.run();
    
    return 0;
}

#endif
