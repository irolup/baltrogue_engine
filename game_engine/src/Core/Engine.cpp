#include "Core/Engine.h"
#include "Scene/SceneManager.h"
#include "Rendering/Renderer.h"
#include "Input/InputManager.h"
#include "Core/Time.h"
#include "Core/MenuManager.h"
#include "Core/ScriptManager.h"
#include "Audio/AudioManager.h"
#include <iostream>

#ifdef EDITOR_BUILD
    #include "Editor/EditorSystem.h"
    #include <GL/glew.h>
    #include <GLFW/glfw3.h>
#endif

namespace GameEngine {

static Engine* s_engineInstance = nullptr;

Engine::Engine()
    : running(false)
    , mode(EngineMode::GAME)
    , sceneManager(nullptr)
    , renderer(nullptr)
    , inputManager(nullptr)
    , timeSystem(nullptr)
#ifdef EDITOR_BUILD
    , editor(nullptr)
#else
    , editor(nullptr)
#endif
{
    s_engineInstance = this;
}

Engine::~Engine() {
    shutdown();
    s_engineInstance = nullptr;
}

bool Engine::initialize(EngineMode engineMode) {
    mode = engineMode;
    
    if (!initializePlatform()) {
        return false;
    }
    
    if (!initializeSystems()) {
        return false;
    }
    
    running = true;
    return true;
}

void Engine::run() {
    while (running) {
        handleEvents();
        update();
        render();
    }
}

void Engine::shutdown() {
    if (!running) return;
    
    running = false;
    
#ifdef EDITOR_BUILD
    if (editor) {
        editor->shutdown();
        editor.reset();
    }
#endif
    
    if (sceneManager) {
        sceneManager.reset();
    }
    
    if (renderer) {
        renderer->shutdown();
        renderer.reset();
    }
    
    if (inputManager) {
        inputManager->shutdown();
        inputManager.reset();
    }
    
    if (timeSystem) {
        timeSystem.reset();
    }
    
    PhysicsManager::getInstance().shutdown();
    
    ScriptManager::getInstance().shutdown();
    
    // Shutdown menu system
    MenuManager::getInstance().shutdown();
    AudioManager::getInstance().shutdown();
    
    platformShutdown();
}

bool Engine::initializePlatform() {
#ifdef LINUX_BUILD
    // On Linux (both Editor and Game), use platformInit()
    return platformInit();
#else
    // On Vita, platformInit() is just a placeholder
    // VitaGL is initialized in main() before Engine creation
    return true;
#endif
}

bool Engine::initializeSystems() {
    timeSystem = std::unique_ptr<Time>(new Time());
    if (!timeSystem->initialize()) {
        return false;
    }
    
    inputManager = std::unique_ptr<InputManager>(new InputManager());
    if (!inputManager->initialize()) {
        return false;
    }
    
    if (!PhysicsManager::getInstance().initialize()) {
        std::cerr << "Failed to initialize physics system!" << std::endl;
        return false;
    }
    
    renderer = std::unique_ptr<Renderer>(new Renderer());
    
    sceneManager = std::unique_ptr<SceneManager>(new SceneManager());
    
    if (!ScriptManager::getInstance().initialize()) {
        std::cerr << "Failed to initialize script system!" << std::endl;
        return false;
    }
    
    if (!MenuManager::getInstance().initialize()) {
        std::cerr << "Failed to initialize menu system!" << std::endl;
        return false;
    }
    
    if (!AudioManager::getInstance().initialize()) {
        std::cerr << "Warning: Failed to initialize audio system (audio will be disabled)" << std::endl;
    }
    
#ifdef EDITOR_BUILD
    if (mode == EngineMode::EDITOR) {
        editor = std::unique_ptr<EditorSystem>(new EditorSystem());
        if (!editor->initialize()) {
            return false;
        }
        
        inputManager->setEditorMode(true);
    }
#endif
    
    return true;
}

Renderer& Engine::getRenderer() {
    static bool rendererInitialized = false;
    if (!rendererInitialized) {
        if (renderer->initialize()) {
            rendererInitialized = true;
        }
    }
    return *renderer;
}

void Engine::update() {
    timeSystem->beginFrame();
    timeSystem->update();
    
    inputManager->update();
    
#ifdef LINUX_BUILD
    if (inputManager->shouldExit()) {
        running = false;
        return;
    }
#endif
    
#ifdef EDITOR_BUILD
    if (editor && mode == EngineMode::EDITOR) {
        editor->update(timeSystem->getDeltaTime());
    }
#endif
    
    MenuManager::getInstance().update(timeSystem->getDeltaTime());
    
    if (sceneManager->getCurrentScene()) {
        sceneManager->update(timeSystem->getDeltaTime());
    }
    
    if (!MenuManager::getInstance().isGamePaused()) {
        PhysicsManager::getInstance().update(timeSystem->getDeltaTime());
    }
    
    if (renderer) {
        renderer->updateLightingUniforms();
    }
    
    timeSystem->endFrame();
}

void Engine::render() {
    renderer->beginFrame();
    
#ifdef EDITOR_BUILD
    if (editor && mode == EngineMode::EDITOR) {
        renderer->setClearColor(0.1f, 0.1f, 0.1f);
        renderer->clear();
        
        editor->render();
    } else {
#endif
        renderer->setClearColor(0.2f, 0.3f, 0.3f);
        renderer->clear();
        
        if (sceneManager->getCurrentScene()) {
            sceneManager->render();
        }
        
        MenuManager::getInstance().render(*renderer);
#ifdef EDITOR_BUILD
    }
#endif
    
    renderer->endFrame();
    renderer->present();
}

void Engine::handleEvents() {
#ifdef LINUX_BUILD
    // Use platform abstraction instead of direct GLFW calls
    // The platform will handle GLFW events internally
    // We'll check for exit through input polling instead
#endif
}

void Engine::setWindowTitle(const std::string& title) {
#ifdef LINUX_BUILD
    // Window title setting is handled by the platform
    // For now, just print the title
    std::cout << "Setting window title: " << title << std::endl;
#endif
}

glm::ivec2 Engine::getWindowSize() const {
#ifdef LINUX_BUILD
    return glm::ivec2(VITA_WIDTH, VITA_HEIGHT);
#endif
    return glm::ivec2(VITA_WIDTH, VITA_HEIGHT);
}

Engine& GetEngine() {
    return *s_engineInstance;
}

} // namespace GameEngine
