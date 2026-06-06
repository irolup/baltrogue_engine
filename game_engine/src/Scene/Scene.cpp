#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Rendering/Renderer.h"
#include "Components/CameraComponent.h"
#include "Components/SkyboxComponent.h"
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

void Scene::queueRemoveNode(const std::string& nodeName) {
    if (nodeName.empty() || nodeName == "Root") return;
    pendingNodeRemovals.push_back(nodeName);
}

void Scene::processPendingRemovals() {
    for (const auto& nodeName : pendingNodeRemovals) {
        auto node = findNode(nodeName);
        if (node && rootNode) {
            rootNode->removeChild(node);
        }
    }
    pendingNodeRemovals.clear();
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
    processPendingRemovals();
    if (rootNode) {
        rootNode->update(deltaTime);
    }
}

void Scene::fixedUpdate(float deltaTime) {
    if (rootNode) {
        rootNode->fixedUpdate(deltaTime);
    }
}

void Scene::lateUpdate(float deltaTime) {
    if (rootNode) {
        rootNode->lateUpdate(deltaTime);
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

void Scene::render(IRenderer& renderer) {
    auto cameras = getActiveGameCameras();
    if (cameras.empty()) return;
    if (cameras.size() == 1) {
        auto cam = cameras[0]->getComponent<CameraComponent>(); 
        glm::ivec4 viewport = cam->getViewport();
        renderer.renderFromCamera(*this, cam, viewport);
        return;
    }

    for (auto& cameraNode : cameras) {
        if (!cameraNode) continue;

        auto cameraComponent = cameraNode->getComponent<CameraComponent>();
        if (!cameraComponent || !cameraComponent->isActive()) continue;

        auto vp = cameraComponent->getViewport();
        if (vp.z <= 0.0f || vp.w <= 0.0f) continue;

        renderer.renderFromCamera(*this, cameraComponent, vp);
    }
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

void Scene::setCameraActive(std::shared_ptr<SceneNode> cameraNode, bool active) {
    if (!cameraNode) return;
    auto cameraComp = cameraNode->getComponent<CameraComponent>();
    if (!cameraComp) return;

    cameraComp->setActive(active);

    if (active) {
        auto& renderer = GetEngine().getRenderer();
        renderer.setActiveCamera(cameraComp);
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

std::vector<std::shared_ptr<SceneNode>> Scene::getActiveGameCameras() const {
    std::vector<std::shared_ptr<SceneNode>> cameras;

    if (!rootNode)
        return cameras;

    std::function<void(std::shared_ptr<SceneNode>)> findCameras =
        [&](std::shared_ptr<SceneNode> node)
    {
        if (!node)
            return;

        auto cameraComponent = node->getComponent<CameraComponent>();
        if (cameraComponent && cameraComponent->isActive()) {
            cameras.push_back(node);
        }

        for (size_t i = 0; i < node->getChildCount(); i++) {
            findCameras(node->getChild(i));
        }
    };

    findCameras(rootNode);

    return cameras;
}

void Scene::setActiveSkybox(std::shared_ptr<SceneNode> skyboxNode) {
    if (skyboxNode && skyboxNode->getComponent<SkyboxComponent>()) {
        std::function<void(std::shared_ptr<SceneNode>)> deactivateAllSkyboxes = 
            [&](std::shared_ptr<SceneNode> node) {
                if (!node) return;
                
                auto skyboxComp = node->getComponent<SkyboxComponent>();
                if (skyboxComp) {
                    skyboxComp->setActive(false);
                }
                
                for (size_t i = 0; i < node->getChildCount(); ++i) {
                    deactivateAllSkyboxes(node->getChild(i));
                }
            };
        
        if (rootNode) {
            deactivateAllSkyboxes(rootNode);
        }
        
        activeSkybox = skyboxNode;
        
        auto skyboxComponent = skyboxNode->getComponent<SkyboxComponent>();
        skyboxComponent->setActive(true);
    } else if (!skyboxNode) {
        activeSkybox.reset();
    }
}

} // namespace GameEngine
