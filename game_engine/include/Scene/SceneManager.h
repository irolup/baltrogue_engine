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
    void render();
    
    std::vector<std::string> getSceneNames() const;
    
    bool saveScene(const std::string& name, const std::string& filepath);
    bool loadSceneFromFile(const std::string& name, const std::string& filepath);
    
private:
    std::shared_ptr<Scene> currentScene;
    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes;
};

} // namespace GameEngine

#endif // SCENE_MANAGER_H
