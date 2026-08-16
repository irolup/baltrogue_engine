#ifndef ENGINE_H
#define ENGINE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Platform.h"
#include "Platform/VitaMath.h"
#include "Scene/SceneManager.h"
#include "Rendering/IRenderer.h"
#include "Rendering/Material.h"
#include "Input/InputManager.h"
#include "Core/Time.h"
#include "Core/MenuManager.h"
#include "Physics/PhysicsManager.h"

#ifdef EDITOR_BUILD
    #include "Editor/EditorSystem.h"
#endif

#ifdef ENABLE_VULKAN
    // Forward declarations for optional Vulkan pieces
    namespace GameEngine {
        class VulkanInstance;
        class VulkanDevice;
        class VulkanSwapChain;
        class VulkanResources;
        class VulkanPipeline;
        class VulkanFrame;
    }
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
    IRenderer& getRenderer(); // Moved to cpp file to handle initialization
    std::shared_ptr<Material> getOrCreateMaterialByShaderPaths(const std::string& vertexPath, const std::string& fragmentPath);
    InputManager& getInputManager() { return *inputManager; }
    Time& getTime() { return *timeSystem; }
    PhysicsManager& getPhysicsManager() { return PhysicsManager::getInstance(); }

    void waitForGpuIdle();

#ifdef ENABLE_VULKAN
    VulkanResources* getVulkanResources() { return vulkanResources.get(); }
    VulkanFrame* getVulkanFrame() { return vulkanFrame.get(); }
    VulkanPipeline* getVulkanPipeline() { return vulkanPipeline.get(); }
#endif
    
#ifdef EDITOR_BUILD
    EditorSystem& getEditor() { return *editor; }
    bool isEditorMode() const { return mode == EngineMode::EDITOR; }
#endif
    
    bool isRunning() const { return running; }
    void setRunning(bool state) { running = state; }
    
    void setWindowTitle(const std::string& title);
    glm::ivec2 getWindowSize() const;

    void setWindowSize(int width, int height);
    void setFullscreen(bool enabled);
    
private:
    bool running;
    EngineMode mode;
    float physicsAccumulator;

    std::unique_ptr<SceneManager> sceneManager;
    std::unique_ptr<IRenderer> renderer;
    std::unique_ptr<InputManager> inputManager;
    std::unique_ptr<Time> timeSystem;

#ifdef ENABLE_VULKAN
    // Optional Vulkan owned objects (only when ENABLE_VULKAN is defined)
    std::unique_ptr<VulkanInstance> vulkanInstance;
    std::unique_ptr<VulkanDevice> vulkanDevice;
    std::unique_ptr<VulkanSwapChain> vulkanSwapChain;
    std::unique_ptr<VulkanResources> vulkanResources;
    std::unique_ptr<VulkanPipeline> vulkanPipeline;
    std::unique_ptr<VulkanFrame> vulkanFrame;
#endif
    std::unordered_map<std::string, std::shared_ptr<Material>> materialCacheByShaderPaths;
    
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
// Null-safe variant for destructors that may run during or after engine shutdown.
Engine* GetEngineIfExists();

} // namespace GameEngine

#endif // ENGINE_H
