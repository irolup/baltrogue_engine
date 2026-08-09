#include "Scene/SceneNode.h"
#include "Scene/Scene.h"
#include "Components/Component.h"
#include "Components/LightComponent.h"
#include "Components/SoundComponent.h"
#include "Rendering/Renderer.h"
#include "Rendering/LightingManager.h"
#include <algorithm>
#include <iostream>
#include "Core/MenuManager.h"
#include "Components/ScriptComponent.h"
#include "Components/TextComponent.h"

namespace GameEngine {

SceneNode::SceneNode(const std::string& name)
    : name(name)
    , parent(nullptr)
    , owningScene(nullptr)
    , visible(true)
    , active(true)
    , selected(false)
{
}

SceneNode::~SceneNode() {
    for (auto& component : components) {
        if (component->getTypeName() == LightComponent::StaticTypeName()) {
            LightingManager::getInstance().removeLight(
                static_cast<LightComponent*>(component.get()));
        }
        component->destroy();
    }
}

void SceneNode::setOwningScene(Scene* scene) {
    owningScene = scene;
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i]) {
            children[i]->setOwningScene(scene);
        }
    }
}

void SceneNode::setName(const std::string& newName) {
    if (name == newName) {
        return;
    }
    if (owningScene) {
        owningScene->onNodeRenamed(this, name, newName);
    }
    name = newName;
}

void SceneNode::start() {
    if (!active) return;

    // Index-based: scripts may add components to this node during start()
    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->isEnabled()) {
            components[i]->start();
        }
    }

    for (size_t i = 0; i < children.size(); ++i) {
        children[i]->start();
    }
}

void SceneNode::suspend() {
    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->isEnabled()) {
            components[i]->suspend();
        }
    }

    for (size_t i = 0; i < children.size(); ++i) {
        children[i]->suspend();
    }
}

void SceneNode::resume() {
    if (!active) return;

    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->isEnabled()) {
            components[i]->resume();
        }
    }

    for (size_t i = 0; i < children.size(); ++i) {
        children[i]->resume();
    }
}

void SceneNode::addChild(std::shared_ptr<SceneNode> child) {
    if (!child || child.get() == this) return;
    
    if (child->parent) {
        child->parent->removeChild(child);
    }
    
    child->parent = this;
    children.push_back(child);
    child->setOwningScene(owningScene);
    if (owningScene) {
        owningScene->registerNodeTree(child);
    }
}

void SceneNode::removeChild(std::shared_ptr<SceneNode> child) {
    if (!child) return;
    
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        if (owningScene) {
            owningScene->unregisterNodeTree(child);
        }
        (*it)->setOwningScene(nullptr);
        (*it)->parent = nullptr;
        children.erase(it);
    }
}

void SceneNode::removeChild(const std::string& childName) {
    auto child = getChild(childName);
    if (child) {
        removeChild(child);
    }
}

void SceneNode::removeAllChildren() {
    for (auto& child : children) {
        if (child) {
            if (owningScene) {
                owningScene->unregisterNodeTree(child);
            }
            child->setOwningScene(nullptr);
            child->parent = nullptr;
        }
    }
    children.clear();
}

std::shared_ptr<SceneNode> SceneNode::getChild(const std::string& childName) {
    for (auto& child : children) {
        if (child->name == childName) {
            return child;
        }
    }
    return nullptr;
}

std::shared_ptr<SceneNode> SceneNode::getChild(size_t index) {
    if (index < children.size()) {
        return children[index];
    }
    return nullptr;
}

glm::mat4 SceneNode::getWorldMatrix() const {
    glm::mat4 worldMatrix = getLocalMatrix();
    
    if (parent) {
        worldMatrix = parent->getWorldMatrix() * worldMatrix;
    }
    
    return worldMatrix;
}

glm::quat SceneNode::getWorldRotation() const {
    glm::quat worldRotation = transform.getRotation();
    if (parent) {
        worldRotation = parent->getWorldRotation() * worldRotation;
    }
    return glm::normalize(worldRotation);
}

glm::vec3 SceneNode::getWorldScale() const {
    glm::vec3 worldScale = transform.getScale();
    if (parent) {
        worldScale *= parent->getWorldScale();
    }
    return worldScale;
}

bool SceneNode::hasComponent(const std::string& typeName) const {
    for (const auto& component : components) {
        if (component->getTypeName() == typeName) {
            return true;
        }
    }
    return false;
}

void SceneNode::update(float deltaTime) {
    if (!active) return;

    bool paused = MenuManager::getInstance().isGamePaused();

    // Index-based: scripts may add components to this node during update()
    for (size_t i = 0; i < components.size(); ++i) {
        auto& component = components[i];
        if (!component->isEnabled()) {
            continue;
        }

        const std::string& typeName = component->getTypeName();
        if (typeName == ScriptComponent::StaticTypeName()) {
            auto* scriptComp = static_cast<ScriptComponent*>(component.get());
            if (paused) {
                if (scriptComp->isPauseExempt()) {
                    scriptComp->update(deltaTime);
                }
            } else {
                scriptComp->update(deltaTime);
            }
        } else if (typeName == SoundComponent::StaticTypeName()) {
            static_cast<SoundComponent*>(component.get())->update(deltaTime);
        } else if (typeName == TextComponent::StaticTypeName()) {
            static_cast<TextComponent*>(component.get())->update(deltaTime);
        } else if (!paused) {
            component->update(deltaTime);
        }
    }

    updateChildren(deltaTime);
}

void SceneNode::fixedUpdate(float deltaTime) {
    if (!active) return;
    bool paused = MenuManager::getInstance().isGamePaused();
    for (size_t i = 0; i < components.size(); ++i) {
        auto& component = components[i];
        if (!component->isEnabled()) {
            continue;
        }

        if (component->getTypeName() == ScriptComponent::StaticTypeName()) {
            auto* scriptComp = static_cast<ScriptComponent*>(component.get());
            if (!paused || scriptComp->isPauseExempt()) {
                scriptComp->fixedUpdate(deltaTime);
            }
        } else if (!paused) {
            component->fixedUpdate(deltaTime);
        }
    }
    fixedUpdateChildren(deltaTime);
}

void SceneNode::lateUpdate(float deltaTime) {
    if (!active) return;

    bool paused = MenuManager::getInstance().isGamePaused();

    for (size_t i = 0; i < components.size(); ++i) {
        auto& component = components[i];
        if (!component->isEnabled()) {
            continue;
        }

        if (component->getTypeName() == ScriptComponent::StaticTypeName()) {
            auto* scriptComp = static_cast<ScriptComponent*>(component.get());
            if (paused) {
                if (scriptComp->isPauseExempt()) {
                    scriptComp->lateUpdate(deltaTime);
                }
            } else {
                scriptComp->lateUpdate(deltaTime);
            }
        } else if (!paused) {
            component->lateUpdate(deltaTime);
        }
    }

    lateUpdateChildren(deltaTime);
}

void SceneNode::render(IRenderer& renderer) {
    if (!visible || !active) return;

    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->isEnabled()) {
            components[i]->render(renderer);
        }
    }

    renderChildren(renderer);
}

std::shared_ptr<SceneNode> SceneNode::findByName(const std::string& nodeName, bool recursive) {
    for (auto& child : children) {
        if (child->name == nodeName) {
            return child;
        }
    }
    
    // Recursively search children if requested
    if (recursive) {
        for (auto& child : children) {
            auto found = child->findByName(nodeName, true);
            if (found) {
                return found;
            }
        }
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<SceneNode>> SceneNode::findByTag(const std::string& tag, bool recursive) {
    std::vector<std::shared_ptr<SceneNode>> result;
    
    for (auto& child : children) {
        if (child->hasTag(tag)) {
            result.push_back(child);
        }
    }
    
    // Recursively search children if requested
    if (recursive) {
        for (auto& child : children) {
            auto childResults = child->findByTag(tag, true);
            result.insert(result.end(), childResults.begin(), childResults.end());
        }
    }
    
    return result;
}

void SceneNode::addTag(const std::string& tag) {
    if (!hasTag(tag)) {
        tags.push_back(tag);
    }
}

void SceneNode::removeTag(const std::string& tag) {
    tags.erase(
        std::remove(tags.begin(), tags.end(), tag),
        tags.end()
    );
}

bool SceneNode::hasTag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

void SceneNode::updateChildren(float deltaTime) {
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i]) children[i]->update(deltaTime);
    }
}

void SceneNode::fixedUpdateChildren(float deltaTime) {
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i]) children[i]->fixedUpdate(deltaTime);
    }
}

void SceneNode::lateUpdateChildren(float deltaTime) {
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i]) children[i]->lateUpdate(deltaTime);
    }
}

void SceneNode::renderChildren(IRenderer& renderer) {
    for (auto& child : children) {
        child->render(renderer);
    }
}

void SceneNode::reorderChild(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= children.size() || toIndex >= children.size() || fromIndex == toIndex) {
        return;
    }
    
    auto childToMove = children[fromIndex];
    
    children.erase(children.begin() + fromIndex);
    
    children.insert(children.begin() + toIndex, childToMove);
}

} // namespace GameEngine
