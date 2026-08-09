#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include <memory>
#include <string>
#include <unordered_map>
#include "Scene/Scene.h"

namespace GameEngine {

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    std::shared_ptr<Scene> createScene(const std::string& name);
    bool loadScene(const std::string& name);
    bool loadScene(std::shared_ptr<Scene> scene);
    void unloadCurrentScene();

    std::shared_ptr<Scene> getCurrentScene() const { return currentScene; }
    std::shared_ptr<Scene> getScene(const std::string& name);
    bool hasScene(const std::string& name) const;

    void update(float deltaTime);
    void fixedUpdate(float deltaTime);
    void lateUpdate(float deltaTime);
    void render();

    std::vector<std::string> getSceneNames() const;

    bool saveScene(const std::string& name, const std::string& filepath);

    bool loadSceneFromFile(const std::string& name, const std::string& filepath);

    // Queue a load for the next frame. Use from Lua so the calling script is not destroyed mid-call
    bool requestLoadSceneFromFile(const std::string& name, const std::string& filepath);

    // Parse and cache a scene without activating it. Safe to call while another scene is running
    bool preloadSceneFromFile(const std::string& name, const std::string& filepath);

    // Apply a queued scene load. Called by the engine at the start of each frame
    void processPendingSceneLoad();

    void flushDeferredGpuRelease();
    void clearSceneCache();

private:
    struct CachedSceneEntry {
        std::shared_ptr<Scene> scene;
        std::string filepath;
    };

    struct PendingSceneLoad {
        bool active = false;
        std::string name;
        std::string filepath;
    };

    static constexpr size_t kMaxCachedScenes = 3;

    std::shared_ptr<Scene> currentScene;
    std::unordered_map<std::string, CachedSceneEntry> cachedScenes;
    PendingSceneLoad pendingSceneLoad_;
    bool deferredGpuRelease_ = false;

    void detachCurrentScene();
    void releaseGpuSceneResources();
    std::shared_ptr<Scene> findCachedScene(const std::string& name, const std::string& filepath) const;
    std::shared_ptr<Scene> loadSceneFromDisk(const std::string& filepath);
    bool activateScene(const std::string& name, std::shared_ptr<Scene> scene, bool fromCache);
    bool finishSceneActivation(std::shared_ptr<Scene> scene, bool fromCache);
    bool loadSceneFromFileInternal(const std::string& name, const std::string& filepath, bool deferActivation);
    void storeCachedScene(const std::string& name, const std::string& filepath, std::shared_ptr<Scene> scene);
    void trimSceneCache(const std::string& activeName);
};

} // namespace GameEngine

#endif // SCENE_MANAGER_H
