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
class IRenderer;
class Scene;
class LightComponent;

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

    Scene* getOwningScene() const { return owningScene; }
    void setOwningScene(Scene* scene);
    
    Transform& getTransform() { return transform; }
    const Transform& getTransform() const { return transform; }
    glm::mat4 getWorldMatrix() const;
    glm::mat4 getLocalMatrix() const { return transform.getMatrix(); }
    glm::quat getWorldRotation() const;
    glm::vec3 getWorldScale() const;
    
    const std::string& getName() const { return name; }
    void setName(const std::string& newName);
    
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
    virtual void suspend();
    virtual void resume();
    virtual void update(float deltaTime);
    virtual void fixedUpdate(float deltaTime);
    virtual void lateUpdate(float deltaTime);
    virtual void render(IRenderer& renderer);
    
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
    Scene* owningScene;
    std::vector<std::shared_ptr<SceneNode>> children;
    std::vector<std::unique_ptr<Component>> components;
    std::vector<std::string> tags;
    
    bool visible;
    bool active;
    bool selected;
    
    void updateChildren(float deltaTime);
    void fixedUpdateChildren(float deltaTime);
    void lateUpdateChildren(float deltaTime);
    void suspendChildren();
    void resumeChildren();
    void renderChildren(IRenderer& renderer);
};

template<typename T, typename... Args>
T* SceneNode::addComponent(Args&&... args) {
    auto component = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    T* ptr = component.get();
    component->setOwner(this);

    Component* basePtr = ptr;
    if (basePtr->getTypeName() == LightComponent::StaticTypeName()) {
        LightingManager::getInstance().addLight(static_cast<LightComponent*>(basePtr));
    }

    components.push_back(std::move(component));
    return ptr;
}

template<typename T>
T* SceneNode::getComponent() {
    const std::string& typeName = T::StaticTypeName();
    for (auto& component : components) {
        if (component->getTypeName() == typeName) {
            return static_cast<T*>(component.get());
        }
    }
    return nullptr;
}

template<typename T>
void SceneNode::removeComponent() {
    const std::string& typeName = T::StaticTypeName();
    for (auto& component : components) {
        if (component->getTypeName() == typeName) {
            if (typeName == LightComponent::StaticTypeName()) {
                LightingManager::getInstance().removeLight(
                    static_cast<LightComponent*>(component.get()));
            }
        }
    }

    components.erase(
        std::remove_if(components.begin(), components.end(),
            [&typeName](const std::unique_ptr<Component>& component) {
                return component->getTypeName() == typeName;
            }),
        components.end());
}

} // namespace GameEngine

#endif // SCENE_NODE_H
