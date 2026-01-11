#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Rendering/Renderer.h"
#include "Core/Engine.h"
#include "Editor/SceneSerializer.h"
#include <iostream>
#include <fstream>

namespace GameEngine {

SceneManager::SceneManager()
    : currentScene(nullptr)
{
}

SceneManager::~SceneManager() {
    unloadCurrentScene();
}

std::shared_ptr<Scene> SceneManager::createScene(const std::string& name) {
    auto scene = std::make_shared<Scene>(name);
    scenes[name] = scene;
    return scene;
}

bool SceneManager::loadScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        currentScene = it->second;
        return true;
    }
    return false;
}

bool SceneManager::loadScene(std::shared_ptr<Scene> scene) {
    if (scene) {
        currentScene = scene;
        if (scenes.find(scene->getName()) == scenes.end()) {
            scenes[scene->getName()] = scene;
        }
        
        scene->start();
        return true;
    }
    return false;
}

void SceneManager::unloadCurrentScene() {
    if (currentScene) {
        currentScene->destroy();
    }
    currentScene.reset();
}

std::shared_ptr<Scene> SceneManager::getScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        return it->second;
    }
    return nullptr;
}

bool SceneManager::hasScene(const std::string& name) const {
    return scenes.find(name) != scenes.end();
}

void SceneManager::update(float deltaTime) {
    if (currentScene) {
        currentScene->update(deltaTime);
    }
}

void SceneManager::render() {
    if (currentScene) {
        auto& renderer = GetEngine().getRenderer();
        currentScene->render(renderer);
    }
}

std::vector<std::string> SceneManager::getSceneNames() const {
    std::vector<std::string> names;
    for (const auto& pair : scenes) {
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

bool SceneManager::loadSceneFromFile(const std::string& name, const std::string& filepath) {
    unloadCurrentScene();
    
    auto scene = SceneSerializer::loadSceneFromFile(filepath);
    if (scene) {
        scene->setName(name);
        scenes[name] = scene;
        return loadScene(scene);
    } else {
#ifdef VITA_BUILD
        printf("SceneManager: Failed to load scene from file: %s\n", filepath.c_str());
#else
        std::cerr << "SceneManager: Failed to load scene from file: " << filepath << std::endl;
#endif
        return false;
    }
}

} // namespace GameEngine
