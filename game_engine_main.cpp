#include "Core/Engine.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"

// Include your original game controller for integration
#include "GameController.h"

using namespace GameEngine;

class GameEngineApp {
public:
    GameEngineApp() : engine(), gameController() {}
    
    bool initialize() {
        // Initialize the engine in game mode
        if (!engine.initialize(EngineMode::GAME)) {
            return false;
        }
        
        engine.setWindowTitle("Game Engine - Your Game");
        
        // Initialize your original game controller if needed
        // You can gradually migrate from GameController to the new engine
        if (!gameController.init()) {
            return false;
        }
        
        createScene();
        return true;
    }
    
    void run() {
        // Main game loop using the new engine
        while (engine.isRunning() && gameController.isRunning()) {
            // Update both systems for now (during migration)
            gameController.update();
            
            // Render using the new engine
            gameController.draw();
            
            // You can gradually move game logic to the scene system
            // engine.getSceneManager().update(engine.getTime().getDeltaTime());
        }
    }
    
    void shutdown() {
        gameController.shutdown();
        engine.shutdown();
    }
    
private:
    Engine engine;
    GameController gameController; // Your original game controller
    
    void createScene() {
        // Example of creating a scene that could represent your game
        auto& sceneManager = engine.getSceneManager();
        auto gameScene = sceneManager.createScene("Game Scene");
        
        // Create a camera node
        auto cameraNode = gameScene->createNode("Main Camera");
        auto camera = cameraNode->addComponent<CameraComponent>();
        cameraNode->getTransform().setPosition(glm::vec3(0, 0, 5));
        gameScene->getRootNode()->addChild(cameraNode);
        gameScene->setActiveCamera(cameraNode);
        
        // Example: Create nodes that represent your game objects
        // This is where you'd migrate your minigames to use the scene system
        
        sceneManager.loadScene(gameScene);
    }
};

int main() {
    GameEngineApp app;
    
    if (!app.initialize()) {
        return -1;
    }
    
    app.run();
    app.shutdown();
    
    return 0;
}
