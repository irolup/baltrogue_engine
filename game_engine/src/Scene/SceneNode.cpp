#include "Scene/SceneNode.h"
#include "Components/Component.h"
#include "Components/LightComponent.h"
#include "Components/SoundComponent.h"
#include "Rendering/Renderer.h"
#include "Rendering/LightingManager.h"
#include <algorithm>
#include <iostream> // Added for debug output
#include "Core/MenuManager.h"
#include "Components/ScriptComponent.h"

namespace GameEngine {

SceneNode::SceneNode(const std::string& name)
    : name(name)
    , parent(nullptr)
    , visible(true)
    , active(true)
    , selected(false)
{
}

SceneNode::~SceneNode() {
    for (auto& component : components) {
        if (component->getTypeName() == "LightComponent") {
            auto& lightingManager = LightingManager::getInstance();
            LightComponent* lightComponent = dynamic_cast<LightComponent*>(component.get());
            if (lightComponent) {
                lightingManager.removeLight(lightComponent);
            }
        }
        component->destroy();
    }
}

void SceneNode::start() {
    if (!active) return;
    
    for (auto& component : components) {
        if (component->isEnabled()) {
            component->start();
        }
    }
    
    for (auto& child : children) {
        child->start();
    }
}

void SceneNode::addChild(std::shared_ptr<SceneNode> child) {
    if (!child || child.get() == this) return;
    
    if (child->parent) {
        child->parent->removeChild(child);
    }
    
    child->parent = this;
    children.push_back(child);
}

void SceneNode::removeChild(std::shared_ptr<SceneNode> child) {
    if (!child) return;
    
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
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
    
    for (auto& component : components) {
        if (component->isEnabled()) {
            if (auto* scriptComp = dynamic_cast<ScriptComponent*>(component.get())) {
                if (paused) {
                    if (scriptComp->isPauseExempt()) {
                        scriptComp->update(deltaTime);
                    }
                } else {
                    scriptComp->update(deltaTime);
                }
            } else if (auto* soundComp = dynamic_cast<SoundComponent*>(component.get())) {
                soundComp->update(deltaTime);
            } else {
                if (!paused) {
                    component->update(deltaTime);
                }
            }
        }
    }

    updateChildren(deltaTime);
}


void SceneNode::render(Renderer& renderer) {
    if (!visible || !active) return;
    
    for (auto& component : components) {
        if (component->isEnabled()) {
            component->render(renderer);
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
    for (auto& child : children) {
        child->update(deltaTime);
    }
}

void SceneNode::renderChildren(Renderer& renderer) {
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
