#ifndef ENGINE_H
#define ENGINE_H

#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "Platform.h"
#include "Platform/VitaMath.h"
#include "Scene/SceneManager.h"
#include "Rendering/Renderer.h"
#include "Input/InputManager.h"
#include "Core/Time.h"
#include "Core/MenuManager.h"
#include "Physics/PhysicsManager.h"

#ifdef EDITOR_BUILD
    #include "Editor/EditorSystem.h"
#endif

namespace GameEngine {

enum class EngineMode {
    GAME,
    EDITOR
};

class Engine {
public:
    Engine();
    ~Engine();
    
    bool initialize(EngineMode mode = EngineMode::GAME);
    void run();
    void shutdown();
    
    SceneManager& getSceneManager() { return *sceneManager; }
    Renderer& getRenderer(); // Moved to cpp file to handle initialization
    InputManager& getInputManager() { return *inputManager; }
    Time& getTime() { return *timeSystem; }
    PhysicsManager& getPhysicsManager() { return PhysicsManager::getInstance(); }
    
#ifdef EDITOR_BUILD
    EditorSystem& getEditor() { return *editor; }
    bool isEditorMode() const { return mode == EngineMode::EDITOR; }
#endif
    
    bool isRunning() const { return running; }
    void setRunning(bool state) { running = state; }
    
    void setWindowTitle(const std::string& title);
    glm::ivec2 getWindowSize() const;
    
private:
    bool running;
    EngineMode mode;
    
    std::unique_ptr<SceneManager> sceneManager;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<InputManager> inputManager;
    std::unique_ptr<Time> timeSystem;
    
#ifdef EDITOR_BUILD
    std::unique_ptr<EditorSystem> editor;
#else
    void* editor;
#endif
    
    bool initializePlatform();
    bool initializeSystems();
    void update();
    void render();
    void handleEvents();
};

Engine& GetEngine();

} // namespace GameEngine

#endif // ENGINE_H
