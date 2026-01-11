#include "Core/Engine.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/ModelRenderer.h"
#include "Components/TextComponent.h"
#include "Components/LightComponent.h"
#include "Components/PhysicsComponent.h"
#include "Rendering/TextureManager.h"
#include <iostream>

using namespace GameEngine;

class LuaTestApp {
public:
    LuaTestApp() : engine() {}
    
    bool initialize() {
        // Initialize the engine
        if (!engine.initialize()) {
            std::cerr << "Failed to initialize engine!" << std::endl;
            return false;
        }

        engine.setWindowTitle("Lua Scripting Test");
        
        // Enable editor mode for mouse and keyboard input
        engine.getInputManager().setEditorMode(true);
        
        // Initialize texture discovery
        auto& textureManager = TextureManager::getInstance();
        textureManager.discoverAllTextures("assets/textures");
        
        createTestScene();
        return true;
    }
    
    void run() {
        // Run the engine (handles the main loop internally)
        engine.run();
    }
    
    
    void shutdown() {
        engine.shutdown();
    }
    
    
private:
    Engine engine;
    
    void createTestScene() {
        std::cout << "Creating Lua test scene..." << std::endl;
        
        // Create a new scene
        auto scene = engine.getSceneManager().createScene("Lua Test Scene");
        
        // Create camera
        auto cameraNode = scene->createNode("Main Camera");
        cameraNode->getTransform().setPosition(glm::vec3(0.0f, 2.0f, 8.0f));
        auto cameraComponent = cameraNode->addComponent<CameraComponent>();
        cameraComponent->setActive(true);
        cameraComponent->enableControls(true);  // Enable WASD movement controls
        scene->getRootNode()->addChild(cameraNode);
        scene->setActiveCamera(cameraNode);
        
        // Create multiple objects for the interactive demo
        
        // Player (football)
        auto player = scene->createNode("Player");
        player->getTransform().setPosition(glm::vec3(0.0f, 2.0f, 12.0f));  // Moved behind camera
        player->getTransform().setScale(glm::vec3(1.0f, 1.0f, 1.0f));
        auto playerRenderer = player->addComponent<ModelRenderer>();
        playerRenderer->loadModel("assets/models/dirty_football_1k.gltf/dirty_football_1k.gltf");
        auto playerPhysics = player->addComponent<PhysicsComponent>();
        playerPhysics->setCollisionShape(CollisionShapeType::SPHERE, glm::vec3(1.5f, 1.5f, 1.5f));
        playerPhysics->setBodyType(PhysicsBodyType::KINEMATIC);
        auto playerScript = player->addComponent<ScriptComponent>();
        playerScript->loadScript("scripts/player_behavior.lua");
        scene->getRootNode()->addChild(player);
        
        // Patrol Enemy (football)
        auto patrolEnemy = scene->createNode("Patrol Enemy");
        patrolEnemy->getTransform().setPosition(glm::vec3(-5.0f, 1.0f, 0.0f));
        patrolEnemy->getTransform().setScale(glm::vec3(1.2f, 1.2f, 1.2f));
        auto patrolRenderer = patrolEnemy->addComponent<ModelRenderer>();
        patrolRenderer->loadModel("assets/models/dirty_football_1k.gltf/dirty_football_1k.gltf");
        auto patrolScript = patrolEnemy->addComponent<ScriptComponent>();
        patrolScript->loadScript("scripts/patrol_behavior.lua");
        scene->getRootNode()->addChild(patrolEnemy);
        
        // Chase Enemy (football)
        auto chaseEnemy = scene->createNode("Chase Enemy");
        chaseEnemy->getTransform().setPosition(glm::vec3(5.0f, 1.0f, 0.0f));
        chaseEnemy->getTransform().setScale(glm::vec3(1.0f, 1.0f, 1.0f));
        auto chaseRenderer = chaseEnemy->addComponent<ModelRenderer>();
        chaseRenderer->loadModel("assets/models/dirty_football_1k.gltf/dirty_football_1k.gltf");
        auto chaseScript = chaseEnemy->addComponent<ScriptComponent>();
        chaseScript->loadScript("scripts/chase_behavior.lua");
        scene->getRootNode()->addChild(chaseEnemy);
        
        // Collectible (lemon)
        auto collectible = scene->createNode("Collectible");
        collectible->getTransform().setPosition(glm::vec3(0.0f, 2.0f, 0.0f));  // Fixed position at Y=2.0, visible to player
        collectible->getTransform().setScale(glm::vec3(5.0f, 5.0f, 5.0f));  // Large but reasonable scale
        auto collectibleRenderer = collectible->addComponent<ModelRenderer>();
        std::cout << "Loading lemon model..." << std::endl;
        bool lemonLoaded = collectibleRenderer->loadModel("assets/models/lemon_1k.gltf/lemon_1k.gltf");
        std::cout << "Lemon model loaded: " << (lemonLoaded ? "SUCCESS" : "FAILED") << std::endl;
        std::cout << "Lemon SceneNode active: " << (collectible->isActive() ? "YES" : "NO") << std::endl;
        // DISABLE physics component completely - it's interfering with Lua script position control
        // We'll use distance-based collision detection in Lua instead
        // auto collectiblePhysics = collectible->addComponent<PhysicsComponent>();
        // collectiblePhysics->setCollisionShape(CollisionShapeType::SPHERE, glm::vec3(2.0f, 2.0f, 2.0f));
        // collectiblePhysics->setBodyType(PhysicsBodyType::KINEMATIC);
        auto collectibleScript = collectible->addComponent<ScriptComponent>();
        collectibleScript->loadScript("scripts/collectible_behavior.lua");
        std::cout << "Adding lemon to scene root..." << std::endl;
        scene->getRootNode()->addChild(collectible);
        std::cout << "Lemon added to scene root successfully" << std::endl;
        
        // Obstacle (football - static)
        auto obstacle = scene->createNode("Obstacle");
        obstacle->getTransform().setPosition(glm::vec3(-2.0f, 0.5f, -2.0f));
        obstacle->getTransform().setScale(glm::vec3(1.3f, 1.3f, 1.3f));
        auto obstacleRenderer = obstacle->addComponent<ModelRenderer>();
        obstacleRenderer->loadModel("assets/models/dirty_football_1k.gltf/dirty_football_1k.gltf");
        auto obstacleScript = obstacle->addComponent<ScriptComponent>();
        obstacleScript->loadScript("scripts/obstacle_behavior.lua");
        scene->getRootNode()->addChild(obstacle);
        
        // Create title text
        auto titleText = scene->createNode("Title Text");
        titleText->getTransform().setPosition(glm::vec3(0.0f, 4.0f, 0.0f));
        
        // Create score display
        auto scoreText = scene->createNode("Score Text");
        scoreText->getTransform().setPosition(glm::vec3(0.0f, 3.5f, 0.0f));
        
        auto textComponent = titleText->addComponent<TextComponent>();
        textComponent->setText("Lua Scripting Test Scene");
        textComponent->setFontPath("assets/fonts/DroidSans.ttf");
        textComponent->setFontSize(24.0f);
        
        auto scoreComponent = scoreText->addComponent<TextComponent>();
        scoreComponent->setText("Score: 0");
        scoreComponent->setFontPath("assets/fonts/DroidSans.ttf");
        scoreComponent->setFontSize(18.0f);
        scoreComponent->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green color
        scoreComponent->setAlignment(TextAlignment::CENTER);
        
        scene->getRootNode()->addChild(titleText);
        scene->getRootNode()->addChild(scoreText);
        
        
        // Create description text
        auto descText = scene->createNode("Description Text");
        descText->getTransform().setPosition(glm::vec3(0.0f, 3.0f, 0.0f));
        
        auto descComponent = descText->addComponent<TextComponent>();
        descComponent->setText("Collect the spinning lemon to score points!");
        descComponent->setFontPath("assets/fonts/DroidSans.ttf");
        descComponent->setFontSize(18.0f);
        descComponent->setColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow color
        descComponent->setAlignment(TextAlignment::CENTER);
        
        scene->getRootNode()->addChild(descText);
        
        // Create light
        auto lightNode = scene->createNode("Main Light");
        lightNode->getTransform().setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
        
        auto lightComponent = lightNode->addComponent<LightComponent>();
        lightComponent->setType(LightType::POINT);
        lightComponent->setColor(glm::vec3(1.0f, 0.8f, 0.6f));
        lightComponent->setIntensity(2.0f);
        lightComponent->setRange(15.0f);
        
        scene->getRootNode()->addChild(lightNode);
        lightComponent->start();
        
        // Start text components
        textComponent->start();
        descComponent->start();
        
        // Load the scene
        engine.getSceneManager().loadScene(scene);
        
        std::cout << "Lua test scene created successfully!" << std::endl;
    }
};

int main() {
    std::cout << "Starting Lua Scripting Test..." << std::endl;
    
    LuaTestApp app;
    
    if (!app.initialize()) {
        std::cerr << "Failed to initialize Lua test app!" << std::endl;
        return -1;
    }
    
        std::cout << "Lua Interactive Demo initialized successfully!" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move camera" << std::endl;
        std::cout << "  Mouse - Look around" << std::endl;
        std::cout << "  SPACE - Move up" << std::endl;
        std::cout << "  SHIFT - Move down" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;
        std::cout << std::endl;
        std::cout << "Collectible Demo Features:" << std::endl;
        std::cout << "  - Player (Football): Complex movement pattern" << std::endl;
        std::cout << "  - Patrol Enemy (Football): Moves back and forth" << std::endl;
        std::cout << "  - Chase Enemy (Football): Moves towards center" << std::endl;
        std::cout << "  - Collectible (Lemon): Spins and floats - COLLECT IT!" << std::endl;
        std::cout << "  - Obstacle (Football): Static with slow rotation" << std::endl;
        std::cout << "  - Score displayed on screen" << std::endl;
        std::cout << "  - Each lemon collected = 10 points" << std::endl;
    
    app.run();
    app.shutdown();
    
    std::cout << "Lua test app shutdown complete!" << std::endl;
    return 0;
}
