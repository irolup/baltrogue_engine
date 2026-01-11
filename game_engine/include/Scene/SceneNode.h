#ifndef SCENE_NODE_H
#define SCENE_NODE_H

#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "Core/Transform.h"
#include "Rendering/LightingManager.h"

namespace GameEngine {

class Component;
class Renderer;

class SceneNode {
public:
    SceneNode(const std::string& name = "Node");
    virtual ~SceneNode();
    
    void addChild(std::shared_ptr<SceneNode> child);
    void removeChild(std::shared_ptr<SceneNode> child);
    void removeChild(const std::string& name);
    void removeAllChildren();
    std::shared_ptr<SceneNode> getChild(const std::string& name);
    std::shared_ptr<SceneNode> getChild(size_t index);
    size_t getChildCount() const { return children.size(); }
    
    void reorderChild(size_t fromIndex, size_t toIndex);
    
    SceneNode* getParent() const { return parent; }
    void setParent(SceneNode* newParent) { parent = newParent; }
    
    Transform& getTransform() { return transform; }
    const Transform& getTransform() const { return transform; }
    glm::mat4 getWorldMatrix() const;
    glm::mat4 getLocalMatrix() const { return transform.getMatrix(); }
    
    const std::string& getName() const { return name; }
    void setName(const std::string& newName) { name = newName; }
    
    bool isVisible() const { return visible; }
    void setVisible(bool state) { visible = state; }
    
    bool isActive() const { return active; }
    void setActive(bool state) { active = state; }
    
    template<typename T, typename... Args>
    T* addComponent(Args&&... args);
    
    template<typename T>
    T* getComponent();
    
    template<typename T>
    void removeComponent();
    
    bool hasComponent(const std::string& typeName) const;
    
    const std::vector<std::unique_ptr<Component>>& getAllComponents() const { return components; }
    
    virtual void start();
    virtual void update(float deltaTime);
    virtual void render(Renderer& renderer);
    
    bool isSelected() const { return selected; }
    void setSelected(bool state) { selected = state; }
    
    std::shared_ptr<SceneNode> findByName(const std::string& name, bool recursive = true);
    std::vector<std::shared_ptr<SceneNode>> findByTag(const std::string& tag, bool recursive = true);
    
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    const std::vector<std::string>& getTags() const { return tags; }
    
protected:
    std::string name;
    Transform transform;
    SceneNode* parent;
    std::vector<std::shared_ptr<SceneNode>> children;
    std::vector<std::unique_ptr<Component>> components;
    std::vector<std::string> tags;
    
    bool visible;
    bool active;
    bool selected;
    
    void updateChildren(float deltaTime);
    void renderChildren(Renderer& renderer);
};

template<typename T, typename... Args>
T* SceneNode::addComponent(Args&&... args) {
    auto component = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    T* ptr = component.get();
    component->setOwner(this);
    
    if (ptr->getTypeName() == "LightComponent") {
        auto& lightingManager = LightingManager::getInstance();
        LightComponent* lightComponent = dynamic_cast<LightComponent*>(ptr);
        if (lightComponent) {
            lightingManager.addLight(lightComponent);
        }
    }
    
    components.push_back(std::move(component));
    return ptr;
}

template<typename T>
T* SceneNode::getComponent() {
    for (auto& component : components) {
        T* casted = dynamic_cast<T*>(component.get());
        if (casted) {
            return casted;
        }
    }
    return nullptr;
}

template<typename T>
void SceneNode::removeComponent() {
    auto it = std::remove_if(components.begin(), components.end(),
        [](const std::unique_ptr<Component>& component) {
            return dynamic_cast<T*>(component.get()) != nullptr;
        });
    
    for (auto iter = components.begin(); iter != it; ++iter) {
        if (dynamic_cast<T*>(iter->get())) {
            if ((*iter)->getTypeName() == "LightComponent") {
                auto& lightingManager = LightingManager::getInstance();
                LightComponent* lightComponent = dynamic_cast<LightComponent*>(iter->get());
                if (lightComponent) {
                    lightingManager.removeLight(lightComponent);
                }
            }
            break;
        }
    }
    
    components.erase(it, components.end());
}

} // namespace GameEngine

#endif // SCENE_NODE_H
