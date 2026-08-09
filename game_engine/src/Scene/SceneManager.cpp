#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Components/ModelRenderer.h"
#include "Rendering/Renderer.h"
#include "Core/Engine.h"
#include "Editor/SceneSerializer.h"
#include "Platform.h"
#ifdef ENABLE_VULKAN
#include "Rendering/Vulkan/VulkanResources.h"
#include "Rendering/Vulkan/VulkanPipeline.h"
#endif
#include <iostream>

namespace GameEngine {

SceneManager::SceneManager()
    : currentScene(nullptr)
{
}

SceneManager::~SceneManager() {
    clearSceneCache();
}

std::shared_ptr<Scene> SceneManager::createScene(const std::string& name) {
    auto scene = std::make_shared<Scene>(name);
    cachedScenes[name] = CachedSceneEntry{scene, ""};
    return scene;
}

bool SceneManager::loadScene(const std::string& name) {
    auto it = cachedScenes.find(name);
    if (it == cachedScenes.end() || !it->second.scene) {
        return false;
    }

    if (!it->second.filepath.empty()) {
        return requestLoadSceneFromFile(name, it->second.filepath);
    }

    // Code-created scene without a source file: switch immediately
    detachCurrentScene();
    releaseGpuSceneResources();
    deferredGpuRelease_ = false;
    return activateScene(name, it->second.scene, true);
}

bool SceneManager::loadScene(std::shared_ptr<Scene> scene) {
    if (!scene) {
        return false;
    }

    currentScene = scene;
    if (cachedScenes.find(scene->getName()) == cachedScenes.end()) {
        cachedScenes[scene->getName()] = CachedSceneEntry{scene, scene->getSourceFilepath()};
    }

    scene->start();
    return true;
}

void SceneManager::unloadCurrentScene() {
    detachCurrentScene();
}

void SceneManager::flushDeferredGpuRelease() {
    if (!deferredGpuRelease_) {
#ifdef ENABLE_VULKAN
        if (auto* resources = GetEngine().getVulkanResources()) {
            resources->drainRetiredGpuResources();
        }
#endif
        return;
    }

    deferredGpuRelease_ = false;
    releaseGpuSceneResources();
}

void SceneManager::releaseGpuSceneResources() {
#ifdef VITA_BUILD
    platformSwapBuffers();
#endif
    GetEngine().waitForGpuIdle();
#ifdef ENABLE_VULKAN
    if (auto* resources = GetEngine().getVulkanResources()) {
        resources->clearSceneGpuCaches();
    }
    if (auto* pipeline = GetEngine().getVulkanPipeline()) {
        pipeline->clearSceneDescriptorCaches();
    }
#endif
}

void SceneManager::detachCurrentScene() {
    if (currentScene) {
#ifdef VITA_BUILD
        platformSwapBuffers();
#endif
        GetEngine().waitForGpuIdle();

        // The renderer must not keep a camera pointer into the detached scene.
        GetEngine().getRenderer().setActiveCamera(nullptr);

        const std::string sceneName = currentScene->getName();
        if (cachedScenes.find(sceneName) != cachedScenes.end()) {
            currentScene->suspend();
        } else {
            currentScene->destroy();
        }
        currentScene.reset();
    }
    deferredGpuRelease_ = true;
}

std::shared_ptr<Scene> SceneManager::getScene(const std::string& name) {
    auto it = cachedScenes.find(name);
    if (it != cachedScenes.end()) {
        return it->second.scene;
    }
    return nullptr;
}

bool SceneManager::hasScene(const std::string& name) const {
    return cachedScenes.find(name) != cachedScenes.end();
}

void SceneManager::update(float deltaTime) {
    if (currentScene && !currentScene->isSuspended()) {
        currentScene->update(deltaTime);
    }
}

void SceneManager::fixedUpdate(float deltaTime) {
    if (currentScene && !currentScene->isSuspended()) {
        currentScene->fixedUpdate(deltaTime);
    }
}

void SceneManager::lateUpdate(float deltaTime) {
    if (currentScene && !currentScene->isSuspended()) {
        currentScene->lateUpdate(deltaTime);
    }
}

void SceneManager::render() {
    if (currentScene && !currentScene->isSuspended()) {
        auto& renderer = GetEngine().getRenderer();
        currentScene->render(renderer);
    }
}

std::vector<std::string> SceneManager::getSceneNames() const {
    std::vector<std::string> names;
    names.reserve(cachedScenes.size());
    for (const auto& pair : cachedScenes) {
        names.push_back(pair.first);
    }
    return names;
}

bool SceneManager::saveScene(const std::string& name, const std::string& filepath) {
    auto scene = getScene(name);
    if (scene) {
        return scene->saveToFile(filepath);
    }
    return false;
}

bool SceneManager::requestLoadSceneFromFile(const std::string& name, const std::string& filepath) {
    if (name.empty() || filepath.empty()) {
        return false;
    }

    pendingSceneLoad_.active = true;
    pendingSceneLoad_.name = name;
    pendingSceneLoad_.filepath = filepath;
    return true;
}

void SceneManager::processPendingSceneLoad() {
    if (!pendingSceneLoad_.active) {
        return;
    }

    PendingSceneLoad request = pendingSceneLoad_;
    pendingSceneLoad_.active = false;
    loadSceneFromFileInternal(request.name, request.filepath, true);
}

bool SceneManager::preloadSceneFromFile(const std::string& name, const std::string& filepath) {
    if (findCachedScene(name, filepath)) {
        return true;
    }

    auto scene = loadSceneFromDisk(filepath);
    if (!scene) {
        return false;
    }

    scene->setSourceFilepath(filepath);
    storeCachedScene(name, filepath, scene);
    trimSceneCache(name);
    return true;
}

bool SceneManager::loadSceneFromFile(const std::string& name, const std::string& filepath) {
    return loadSceneFromFileInternal(name, filepath, false);
}

bool SceneManager::loadSceneFromFileInternal(const std::string& name, const std::string& filepath, bool deferActivation) {
    (void)deferActivation;
    detachCurrentScene();
    releaseGpuSceneResources();
    deferredGpuRelease_ = false;

    std::shared_ptr<Scene> scene = findCachedScene(name, filepath);
    const bool fromCache = static_cast<bool>(scene);

    if (!fromCache) {
        scene = loadSceneFromDisk(filepath);
        if (!scene) {
#ifdef VITA_BUILD
            printf("SceneManager: Failed to load scene from file: %s\n", filepath.c_str());
#else
            std::cerr << "SceneManager: Failed to load scene from file: " << filepath << std::endl;
#endif
            return false;
        }
        scene->setSourceFilepath(filepath);
    }

    storeCachedScene(name, filepath, scene);
    trimSceneCache(name);
    return activateScene(name, scene, fromCache);
}

void SceneManager::clearSceneCache() {
    pendingSceneLoad_.active = false;
    detachCurrentScene();

    for (auto& entry : cachedScenes) {
        if (entry.second.scene) {
            entry.second.scene->releaseScriptRuntime();
            entry.second.scene->destroy();
        }
    }
    cachedScenes.clear();

    // No scenes remain: drop the CPU-side model cache too
    ModelRenderer::clearMeshCache();
}

std::shared_ptr<Scene> SceneManager::findCachedScene(const std::string& name, const std::string& filepath) const {
    auto it = cachedScenes.find(name);
    if (it == cachedScenes.end() || !it->second.scene) {
        return nullptr;
    }
    if (it->second.filepath != filepath) {
        return nullptr;
    }
    return it->second.scene;
}

std::shared_ptr<Scene> SceneManager::loadSceneFromDisk(const std::string& filepath) {
    return SceneSerializer::loadSceneFromFile(filepath);
}

bool SceneManager::activateScene(const std::string& name, std::shared_ptr<Scene> scene, bool fromCache) {
    if (!scene) {
        return false;
    }

    scene->setName(name);
    currentScene = scene;

    GetEngine().waitForGpuIdle();
    const bool ok = finishSceneActivation(scene, fromCache);
    GetEngine().waitForGpuIdle();
    return ok;
}

bool SceneManager::finishSceneActivation(std::shared_ptr<Scene> scene, bool fromCache) {
    if (!scene) {
        return false;
    }

    if (fromCache && scene->hasEverStarted()) {
        scene->resume();
    } else {
        scene->start();
        scene->markEverStarted();
    }

    // Point the renderer at the activated scene's camera
    if (auto cameraNode = scene->getActiveGameCamera()) {
        if (auto* cameraComponent = cameraNode->getComponent<CameraComponent>()) {
            GetEngine().getRenderer().setActiveCamera(cameraComponent);
        }
    }

    return true;
}

void SceneManager::storeCachedScene(const std::string& name, const std::string& filepath, std::shared_ptr<Scene> scene) {
    cachedScenes[name] = CachedSceneEntry{scene, filepath};
}

void SceneManager::trimSceneCache(const std::string& activeName) {
    while (cachedScenes.size() > kMaxCachedScenes) {
        bool removedAny = false;

        for (auto it = cachedScenes.begin(); it != cachedScenes.end(); ++it) {
            if (it->first == activeName) {
                continue;
            }

            if (it->second.scene) {
                GetEngine().waitForGpuIdle();
                it->second.scene->releaseScriptRuntime();
                it->second.scene->destroy();
            }
            cachedScenes.erase(it);
            removedAny = true;
            break;
        }

        if (!removedAny) {
            break;
        }
    }
}

} // namespace GameEngine
