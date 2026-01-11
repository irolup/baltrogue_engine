
#ifdef LINUX_BUILD

#include "../game_engine/include/Core/Engine.h"
#include "../game_engine/include/Scene/Scene.h"
#include "../game_engine/include/Scene/SceneNode.h"
#include "../game_engine/include/Components/CameraComponent.h"
#include "../game_engine/include/Components/MeshRenderer.h"
#include "../game_engine/include/Components/ModelRenderer.h"
#include "../game_engine/include/Components/LightComponent.h"
#include "../game_engine/include/Components/PhysicsComponent.h"
#include "../game_engine/include/Components/TextComponent.h"
#include "../game_engine/include/Components/ScriptComponent.h"
#include "../game_engine/include/Components/Area3DComponent.h"
#include "../game_engine/include/Rendering/Mesh.h"
#include "../game_engine/include/Rendering/Material.h"
#include "../game_engine/include/Rendering/TextureManager.h"
#include <iostream>

using namespace GameEngine;

int main() {
    // Create engine instance
    Engine engine;
    
    // Initialize in game mode (but we'll create the editor scene)
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize game engine!" << std::endl;
        return -1;
    }
    
    engine.setWindowTitle("Game Engine - Linux Game Build");
    
    // Enable editor mode for keyboard and mouse input
#ifndef VITA_BUILD
    engine.getInputManager().setEditorMode(true);
#endif
    
    // Initialize texture discovery
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
    
    // Create the same scene as the editor (camera + shapes)
    auto& sceneManager = engine.getSceneManager();
    auto gameScene = sceneManager.createScene("Game Scene");
    
    // Add default directional light
    auto lightNode = gameScene->createNode("Default Light");
    auto lightComponent = lightNode->addComponent<LightComponent>();
    lightComponent->setType(LightType::DIRECTIONAL);
    lightComponent->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
    lightComponent->setIntensity(1.0f);
    lightComponent->setRange(100.0f);
    lightComponent->setShowGizmo(false);
    lightNode->getTransform().setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    lightNode->getTransform().setEulerAngles(glm::vec3(-45.0f, 0.0f, 0.0f));
    gameScene->getRootNode()->addChild(lightNode);
    lightComponent->start();

    // Create Main Camera as a child of Player (so it moves with PlayerRoot -> Player)
    // Scene structure: PlayerRoot (empty, move this) -> Player (PhysicsComponent) -> Main Camera
    auto cameraNode = gameScene->createNode("Main Camera");
    auto cameraComponent = cameraNode->addComponent<CameraComponent>();
    cameraComponent->setFOV(45.000000f);
    cameraComponent->setNearPlane(0.100000f);
    cameraComponent->setFarPlane(1000.000000f);
    // Camera controls are now handled by Lua scripts only, not C++ code
    // cameraComponent->enableControls(false);  // Disabled - scripts control movement
    cameraNode->getTransform().setPosition(glm::vec3(0.000000f, 0.000000f, 5.000000f));  // Local position relative to Player
    cameraNode->getTransform().setEulerAngles(glm::vec3(0.000000f, -0.000000f, 0.000000f));
    gameScene->getRootNode()->addChild(cameraNode);
    gameScene->setActiveCamera(cameraNode);

    // Add a text component
    auto textNode1 = gameScene->createNode("MainMenuStart");
    textNode1->getTransform().setPosition(glm::vec3(0.000000f, 3.000000f, 0.000000f));
    textNode1->getTransform().setEulerAngles(glm::vec3(0.000000f, -0.000000f, 0.000000f));
    textNode1->getTransform().setScale(glm::vec3(1.000000f, 1.000000f, 1.000000f));
    auto textComponent1 = textNode1->addComponent<TextComponent>();
    textComponent1->setText("Start Game");
    textComponent1->setFontPath("assets/fonts/DroidSans.ttf");
    textComponent1->setFontSize(32.000000f);
    textComponent1->setColor(glm::vec4(1.000000f, 1.000000f, 1.000000f, 1.000000f));
    textComponent1->setRenderMode(TextRenderMode::SCREEN_SPACE);
    textComponent1->setAlignment(TextAlignment::CENTER);
    textComponent1->setScale(1.000000f);
    textComponent1->setLineSpacing(1.200000f);
    gameScene->getRootNode()->addChild(textNode1);
    textComponent1->start();

    // Add a text component
    auto textNode2 = gameScene->createNode("MainMenuLoad");
    textNode2->getTransform().setPosition(glm::vec3(0.000000f, 1.000000f, 0.000000f));
    textNode2->getTransform().setEulerAngles(glm::vec3(0.000000f, -0.000000f, 0.000000f));
    textNode2->getTransform().setScale(glm::vec3(1.000000f, 1.000000f, 1.000000f));
    auto textComponent2 = textNode2->addComponent<TextComponent>();
    textComponent2->setText("Load Game");
    textComponent2->setFontPath("assets/fonts/DroidSans.ttf");
    textComponent2->setFontSize(32.000000f);
    textComponent2->setColor(glm::vec4(1.000000f, 1.000000f, 1.000000f, 1.000000f));
    textComponent2->setRenderMode(TextRenderMode::SCREEN_SPACE);
    textComponent2->setAlignment(TextAlignment::CENTER);
    textComponent2->setScale(1.000000f);
    textComponent2->setLineSpacing(1.200000f);
    gameScene->getRootNode()->addChild(textNode2);
    textComponent2->start();

    // Add a text component
    auto textNode3 = gameScene->createNode("MainMenuOptions");
    textNode3->getTransform().setPosition(glm::vec3(0.000000f, -1.000000f, 0.000000f));
    textNode3->getTransform().setEulerAngles(glm::vec3(0.000000f, -0.000000f, 0.000000f));
    textNode3->getTransform().setScale(glm::vec3(1.000000f, 1.000000f, 1.000000f));
    auto textComponent3 = textNode3->addComponent<TextComponent>();
    textComponent3->setText("Options");
    textComponent3->setFontPath("assets/fonts/DroidSans.ttf");
    textComponent3->setFontSize(32.000000f);
    textComponent3->setColor(glm::vec4(1.000000f, 1.000000f, 1.000000f, 1.000000f));
    textComponent3->setRenderMode(TextRenderMode::SCREEN_SPACE);
    textComponent3->setAlignment(TextAlignment::CENTER);
    textComponent3->setScale(1.000000f);
    textComponent3->setLineSpacing(1.200000f);
    gameScene->getRootNode()->addChild(textNode3);
    textComponent3->start();

    // Add a text component
    auto textNode4 = gameScene->createNode("MainMenuQuit");
    textNode4->getTransform().setPosition(glm::vec3(0.000000f, -3.000000f, 0.000000f));
    textNode4->getTransform().setEulerAngles(glm::vec3(0.000000f, -0.000000f, 0.000000f));
    textNode4->getTransform().setScale(glm::vec3(1.000000f, 1.000000f, 1.000000f));
    auto textComponent4 = textNode4->addComponent<TextComponent>();
    textComponent4->setText("Quit");
    textComponent4->setFontPath("assets/fonts/DroidSans.ttf");
    textComponent4->setFontSize(32.000000f);
    textComponent4->setColor(glm::vec4(1.000000f, 1.000000f, 1.000000f, 1.000000f));
    textComponent4->setRenderMode(TextRenderMode::SCREEN_SPACE);
    textComponent4->setAlignment(TextAlignment::CENTER);
    textComponent4->setScale(1.000000f);
    textComponent4->setLineSpacing(1.200000f);
    gameScene->getRootNode()->addChild(textNode4);
    textComponent4->start();

    // Add a text component
    auto textNode5 = gameScene->createNode("MainMenuSelector");
    textNode5->getTransform().setPosition(glm::vec3(-10.000000f, 3.000000f, 0.000000f));
    textNode5->getTransform().setEulerAngles(glm::vec3(0.000000f, -0.000000f, 0.000000f));
    textNode5->getTransform().setScale(glm::vec3(1.000000f, 1.000000f, 1.000000f));
    auto textComponent5 = textNode5->addComponent<TextComponent>();
    textComponent5->setText("=>");
    textComponent5->setFontPath("assets/fonts/DroidSans.ttf");
    textComponent5->setFontSize(32.000000f);
    textComponent5->setColor(glm::vec4(1.000000f, 1.000000f, 1.000000f, 1.000000f));
    textComponent5->setRenderMode(TextRenderMode::SCREEN_SPACE);
    textComponent5->setAlignment(TextAlignment::CENTER);
    textComponent5->setScale(1.000000f);
    textComponent5->setLineSpacing(1.200000f);
    gameScene->getRootNode()->addChild(textNode5);
    textComponent5->start();

    // Add a script component
    auto scriptNode1 = gameScene->createNode("Main Menu Controller");
    auto scriptComponent1 = scriptNode1->addComponent<ScriptComponent>();
    scriptComponent1->loadScript("scripts/main_menu.lua");
    scriptComponent1->setPauseExempt(true);
    gameScene->getRootNode()->addChild(scriptNode1);
    scriptComponent1->start();

    std::cout << "Main menu scene loaded!" << std::endl;

    // Try to load main menu scene from JSON file first
    // If it doesn't exist, fall back to loading the current scene
    std::cout << "Attempting to load main menu from assets/scenes/main_menu.json..." << std::endl;
    if (sceneManager.loadSceneFromFile("Main Menu", "assets/scenes/main_menu.json")) {
        std::cout << "Main menu loaded from assets/scenes/main_menu.json" << std::endl;
        std::cout << "Use W/S or Up/Down arrows to navigate, Enter/Space to select." << std::endl;
    } else {
        std::cout << "Main menu scene not found or failed to load. Loading current scene..." << std::endl;
        // Load the current scene (main menu created in code above)
        sceneManager.loadScene(gameScene);
        std::cout << "Current scene loaded (main menu from code)." << std::endl;
    }
    
    // Run the game
    engine.run();
    
    return 0;
}

#endif // LINUX_BUILD
