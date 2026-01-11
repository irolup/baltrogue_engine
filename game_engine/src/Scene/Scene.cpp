#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Rendering/Renderer.h"
#include "Components/CameraComponent.h"
#include "Core/Engine.h"

namespace GameEngine {

Scene::Scene(const std::string& name)
    : name(name)
    , nodeCounter(0)
{
    rootNode = std::make_shared<SceneNode>("Root");
}

Scene::~Scene() {
}

std::shared_ptr<SceneNode> Scene::createNode(const std::string& nodeName) {
    std::string uniqueName = nodeName.empty() ? generateUniqueName("Node") : generateUniqueName(nodeName);
    return std::make_shared<SceneNode>(uniqueName);
}

void Scene::addNode(std::shared_ptr<SceneNode> node) {
    if (node && rootNode) {
        rootNode->addChild(node);
    }
}

void Scene::removeNode(std::shared_ptr<SceneNode> node) {
    if (node && rootNode) {
        rootNode->removeChild(node);
    }
}

void Scene::removeNode(const std::string& nodeName) {
    auto node = findNode(nodeName);
    if (node) {
        removeNode(node);
    }
}

std::shared_ptr<SceneNode> Scene::findNode(const std::string& nodeName) {
    if (!rootNode) return nullptr;
    
    if (rootNode->getName() == nodeName) {
        return rootNode;
    }
    
    return rootNode->findByName(nodeName, true);
}

std::vector<std::shared_ptr<SceneNode>> Scene::findNodesByTag(const std::string& tag) {
    if (!rootNode) return {};
    
    return rootNode->findByTag(tag, true);
}

void Scene::start() {
    if (rootNode) {
        rootNode->start();
    }
}

void Scene::update(float deltaTime) {
    if (rootNode) {
        rootNode->update(deltaTime);
    }
}

void Scene::destroy() {
    if (rootNode) {
        std::function<void(std::shared_ptr<SceneNode>)> destroyNode = 
            [&](std::shared_ptr<SceneNode> node) {
                if (!node) return;
                
                for (size_t i = 0; i < node->getChildCount(); ++i) {
                    destroyNode(node->getChild(i));
                }
                
                const auto& components = node->getAllComponents();
                for (const auto& component : components) {
                    if (component && component->isEnabled()) {
                        component->destroy();
                    }
                }
            };
        
        destroyNode(rootNode);
    }
}

void Scene::render(Renderer& renderer) {
    auto activeGameCamera = getActiveGameCamera();
    if (activeGameCamera) {
        auto cameraComponent = activeGameCamera->getComponent<CameraComponent>();
        if (cameraComponent) {
            renderer.setActiveCamera(cameraComponent);
        }
    }
    
    renderer.renderScene(*this);
}

void Scene::setActiveCamera(std::shared_ptr<SceneNode> cameraNode) {
    if (cameraNode && cameraNode->getComponent<CameraComponent>()) {
        std::function<void(std::shared_ptr<SceneNode>)> deactivateAllCameras = 
            [&](std::shared_ptr<SceneNode> node) {
                if (!node) return;
                
                auto cameraComp = node->getComponent<CameraComponent>();
                if (cameraComp) {
                    cameraComp->setActive(false);
                }
                
                for (size_t i = 0; i < node->getChildCount(); ++i) {
                    deactivateAllCameras(node->getChild(i));
                }
            };
        
        if (rootNode) {
            deactivateAllCameras(rootNode);
        }
        
        activeCamera = cameraNode;
        
        auto cameraComponent = cameraNode->getComponent<CameraComponent>();
        auto& renderer = GetEngine().getRenderer();
        renderer.setActiveCamera(cameraComponent);
        
        cameraComponent->setActive(true);
    }
}

void Scene::setSelectedNode(std::shared_ptr<SceneNode> node) {
    auto prevSelected = selectedNode.lock();
    if (prevSelected) {
        prevSelected->setSelected(false);
    }
    
    selectedNode = node;
    if (node) {
        node->setSelected(true);
    }
}

void Scene::clearSelection() {
    auto selected = selectedNode.lock();
    if (selected) {
        selected->setSelected(false);
    }
    selectedNode.reset();
}

size_t Scene::getNodeCount() const {
    if (!rootNode) return 0;
    return countNodes(rootNode);
}

bool Scene::saveToFile(const std::string& filepath) const {
    // TODO: Implement scene serialization
    return false;
}

bool Scene::loadFromFile(const std::string& filepath) {
    // TODO: Implement scene deserialization
    return false;
}

std::string Scene::generateUniqueName(const std::string& baseName) {
    std::string name = baseName;
    int counter = 1;
    
    while (findNode(name)) {
        name = baseName + "_" + std::to_string(counter);
        counter++;
    }
    
    return name;
}

size_t Scene::countNodes(const std::shared_ptr<SceneNode>& node) const {
    if (!node) return 0;
    
    size_t count = 1; // Count this node
    
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        count += countNodes(node->getChild(i));
    }
    
    return count;
}

std::shared_ptr<SceneNode> Scene::getActiveGameCamera() const {
    if (!rootNode) return nullptr;
    
    std::function<std::shared_ptr<SceneNode>(std::shared_ptr<SceneNode>)> findActiveCamera = 
        [&](std::shared_ptr<SceneNode> node) -> std::shared_ptr<SceneNode> {
            if (!node) return nullptr;
            
            auto cameraComponent = node->getComponent<CameraComponent>();
            if (cameraComponent && cameraComponent->isActive()) {
                return node;
            }
            
            for (size_t i = 0; i < node->getChildCount(); ++i) {
                auto found = findActiveCamera(node->getChild(i));
                if (found) return found;
            }
            return nullptr;
        };
    
    return findActiveCamera(rootNode);
}

} // namespace GameEngine
