#ifdef LINUX_BUILD

#include "../game_engine/include/Core/Engine.h"
#include "../game_engine/include/Scene/Scene.h"
#include "../game_engine/include/Scene/SceneNode.h"
#include "../game_engine/include/Components/CameraComponent.h"
#include "../game_engine/include/Components/TextComponent.h"
#include "../game_engine/include/Rendering/TextureManager.h"
#include <iostream>

using namespace GameEngine;

int main() {
    // Create engine instance
    Engine engine;
    
    // Initialize in game mode
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize game engine!" << std::endl;
        return -1;
    }
    
    engine.setWindowTitle("Text Component Test - Linux Build");
    
    // Initialize texture discovery
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
    
    // Create a test scene
    auto& sceneManager = engine.getSceneManager();
    auto testScene = sceneManager.createScene("Text Test Scene");
    
    // Create a camera
    auto cameraNode = testScene->createNode("Main Camera");
    auto cameraComponent = cameraNode->addComponent<CameraComponent>();
    cameraNode->getTransform().setPosition(glm::vec3(0.0f, 1.0f, 5.0f));
    cameraNode->getTransform().setEulerAngles(glm::vec3(0.0f, 0.0f, 0.0f));
    testScene->getRootNode()->addChild(cameraNode);
    testScene->setActiveCamera(cameraNode);

    // Create world space text
    auto worldTextNode = testScene->createNode("World Space Text");
    auto worldTextComponent = worldTextNode->addComponent<TextComponent>();
    worldTextComponent->setText("Hello World!\nThis is world space text.");
    worldTextComponent->setFontPath("assets/fonts/DroidSans.ttf");
    worldTextComponent->setFontSize(64.0f);
    worldTextComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    worldTextComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
    worldTextComponent->setAlignment(TextAlignment::CENTER);
    worldTextNode->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    testScene->getRootNode()->addChild(worldTextNode);

    // Create screen space text (child of camera)
    auto screenTextNode = testScene->createNode("Screen Space Text");
    auto screenTextComponent = screenTextNode->addComponent<TextComponent>();
    screenTextComponent->setText("Screen Space Text\nThis follows the camera!");
    screenTextComponent->setFontPath("assets/fonts/DroidSans.ttf");
    screenTextComponent->setFontSize(48.0f);
    screenTextComponent->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    screenTextComponent->setRenderMode(TextRenderMode::SCREEN_SPACE);
    screenTextComponent->setAlignment(TextAlignment::CENTER);
    screenTextNode->getTransform().setPosition(glm::vec3(0.0f, 2.0f, 0.0f));
    cameraNode->addChild(screenTextNode); // Make it a child of camera

    // Create another world space text with different properties
    auto coloredTextNode = testScene->createNode("Colored Text");
    auto coloredTextComponent = coloredTextNode->addComponent<TextComponent>();
    coloredTextComponent->setText("Colored Text Example\nWith multiple lines!");
    coloredTextComponent->setFontPath("assets/fonts/DroidSans.ttf");
    coloredTextComponent->setFontSize(32.0f);
    coloredTextComponent->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    coloredTextComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
    coloredTextComponent->setAlignment(TextAlignment::LEFT);
    coloredTextNode->getTransform().setPosition(glm::vec3(-3.0f, -1.0f, 0.0f));
    testScene->getRootNode()->addChild(coloredTextNode);

    // Set the active scene
    sceneManager.loadScene(testScene);
    
    std::cout << "Text Component Test Scene Created!" << std::endl;
    std::cout << "- World space text should appear in 3D space" << std::endl;
    std::cout << "- Screen space text should follow the camera" << std::endl;
    std::cout << "- Use WASD to move camera, mouse to look around" << std::endl;
    
    // Run the game loop
    engine.run();
    
    return 0;
}

#endif
