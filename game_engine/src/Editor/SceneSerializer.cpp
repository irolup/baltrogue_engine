#include "Editor/SceneSerializer.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/LightComponent.h"
#include "Components/PhysicsComponent.h"
#include "Components/TextComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/Area3DComponent.h"
#include "Components/AnimationComponent.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/TextureManager.h"
// Using nlohmann/json library for proper JSON serialization
#include "../../vendor/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>

#ifdef LINUX_BUILD
#include <fstream>
#include <filesystem>
#elif defined(VITA_BUILD)
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <cstring>
#include <vector>
#endif

namespace GameEngine {

std::string SceneSerializer::escapeStringForCpp(const std::string& input) {
    std::string result;
    for (char c : input) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string SceneSerializer::generateVisibilityCode(const std::string& nodeName, std::shared_ptr<SceneNode> node) {
    std::string code;
    if (!node->isVisible()) {
        code += "    " + nodeName + "->setVisible(false);\n";
    }
    if (!node->isActive()) {
        code += "    " + nodeName + "->setActive(false);\n";
    }
    return code;
}

#ifdef LINUX_BUILD
void SceneSerializer::saveSceneToGame(std::shared_ptr<Scene> scene) {
    if (!scene) return;
    
    std::cout << "Saving current scene to game source..." << std::endl;
    
    auto discoveredTextures = discoverAndGenerateTextureAssets();
    
    generateInputMappingAssets();
    
    std::string gameMainContent = generateGameMainContent(scene, discoveredTextures);
    std::string vitaMainContent = generateVitaMainContent(scene, discoveredTextures);
    
    // Write game_main.cpp
    std::ofstream gameMainFile("src/game_main.cpp");
    if (gameMainFile.is_open()) {
        gameMainFile << gameMainContent;
        gameMainFile.close();
        std::cout << "Scene saved to src/game_main.cpp successfully!" << std::endl;
    } else {
        std::cerr << "Failed to open src/game_main.cpp for writing!" << std::endl;
    }
    
    std::ofstream vitaMainFile("src/vita_main.cpp");
    if (vitaMainFile.is_open()) {
        vitaMainFile << vitaMainContent;
        vitaMainFile.close();
        std::cout << "Scene also saved to src/vita_main.cpp successfully!" << std::endl;
    } else {
        std::cerr << "Failed to open src/vita_main.cpp for writing!" << std::endl;
    }
    
    updateMakefileWithTextures(discoveredTextures);
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
std::string SceneSerializer::generateGameMainContent(std::shared_ptr<Scene> scene, const std::vector<std::string>& discoveredTextures) {
    if (!scene) return "";
    
    std::string content = R"(
#ifdef LINUX_BUILD

#include "../game_engine/include/Core/Engine.h"
#include "../game_engine/include/Scene/Scene.h"
#include "../game_engine/include/Scene/SceneNode.h"
#include "../game_engine/include/Components/CameraComponent.h"
#include "../game_engine/include/Components/MeshRenderer.h"
#include "../game_engine/include/Components/ModelRenderer.h"
#include "../game_engine/include/Components/LightComponent.h"
#include "../game_engine/include/Components/PhysicsComponent.h"
#include "../game_engine/include/Components/TextComponent.h"
#include "../game_engine/include/Components/ScriptComponent.h"
#include "../game_engine/include/Components/Area3DComponent.h"
#include "../game_engine/include/Rendering/Mesh.h"
#include "../game_engine/include/Rendering/Material.h"
#include "../game_engine/include/Rendering/TextureManager.h"
#include <iostream>

using namespace GameEngine;

int main() {
    Engine engine;
    
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize game engine!" << std::endl;
        return -1;
    }
    
    engine.setWindowTitle("Game Engine - Linux Game Build");
    
#ifndef VITA_BUILD
    engine.getInputManager().setEditorMode(true);
#endif
    
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
    
    auto& sceneManager = engine.getSceneManager();
    auto gameScene = sceneManager.createScene("Game Scene");
    
)";
    
    int shapeCounter = 0;
    if (scene) {
        auto allNodes = getAllSceneNodesFromScene(scene);
        std::map<SceneNode*, std::string> nodeNameMap;
        
        int physicsCounter = 0;
        int lightCounter = 0;
        
        // Add a default directional light if no lights exist in the scene
        bool hasLights = false;
        for (const auto& node : allNodes) {
            if (node->getComponent<LightComponent>()) {
                hasLights = true;
                break;
            }
        }
        
        if (!hasLights) {
            content += "    // Add default directional light\n";
            content += "    auto lightNode = gameScene->createNode(\"Default Light\");\n";
            content += "    auto lightComponent = lightNode->addComponent<LightComponent>();\n";
            content += "    lightComponent->setType(LightType::DIRECTIONAL);\n";
            content += "    lightComponent->setColor(glm::vec3(1.0f, 1.0f, 1.0f));\n";
            content += "    lightComponent->setIntensity(1.0f);\n";
            content += "    lightComponent->setRange(100.0f);\n";
            content += "    lightComponent->setShowGizmo(false);\n";
            content += "    lightNode->getTransform().setPosition(glm::vec3(0.0f, 10.0f, 0.0f));\n";
            content += "    lightNode->getTransform().setEulerAngles(glm::vec3(-45.0f, 0.0f, 0.0f));\n";
            content += "    gameScene->getRootNode()->addChild(lightNode);\n";
            content += "    lightComponent->start();\n\n";
        }
        
        std::sort(allNodes.begin(), allNodes.end(), [](const std::shared_ptr<SceneNode>& a, const std::shared_ptr<SceneNode>& b) {
            if (!a || !b) return false;
            
            int depthA = 0, depthB = 0;
            SceneNode* currentA = a->getParent();
            SceneNode* currentB = b->getParent();
            
            while (currentA) { depthA++; currentA = currentA->getParent(); }
            while (currentB) { depthB++; currentB = currentB->getParent(); }
            
            return depthA < depthB; // Process parents first
        });
        
        // First pass: Create all nodes (including empty ones) to establish the hierarchy
        for (auto& node : allNodes) {
            if (node && node != scene->getRootNode()) {
                std::string nodeName;
                
                // Determine node type and name
                if (node->getComponent<MeshRenderer>()) {
                    // This will be handled in the second pass
                    continue;
                } else if (node->getComponent<ModelRenderer>()) {
                    // This will be handled in the model pass
                    continue;
                } else if (node->getComponent<LightComponent>()) {
                    // This will be handled in the third pass
                    continue;
                } else if (node->getComponent<CameraComponent>()) {
                    // Camera is handled separately
                    continue;
                } else if (node->getComponent<PhysicsComponent>()) {
                    // Physics component is handled in a separate pass
                    continue;
                } else if (node->getComponent<TextComponent>()) {
                    // Text component is handled in a separate pass
                    continue;
                } else if (node->getComponent<Area3DComponent>()) {
                    // Area3D component is handled in a separate pass
                    continue;
                } else if (node->getComponent<ScriptComponent>()) {
                    // Script component is handled in a separate pass
                    continue;
                } else {
                    // This is an empty node - create it
                    nodeName = "emptyNode" + std::to_string(nodeNameMap.size());
                    nodeNameMap[node.get()] = nodeName;
                    
                    auto& transform = node->getTransform();
                    auto pos = transform.getPosition();
                    auto rot = transform.getEulerAngles();
                    auto scale = transform.getScale();
                    
                    content += "    // Add an empty node\n";
                    content += "    auto " + nodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                    content += "    " + nodeName + "->getTransform().setPosition(glm::vec3(" + 
                              std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                    content += "    " + nodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                              std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                    content += "    " + nodeName + "->getTransform().setScale(glm::vec3(" + 
                              std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                    
                    content += generateVisibilityCode(nodeName, node);
                    
                    if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                        auto parentIt = nodeNameMap.find(node->getParent());
                        if (parentIt != nodeNameMap.end()) {
                            content += "    " + parentIt->second + "->addChild(" + nodeName + ");\n\n";
                        } else {
                            content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                        }
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                    }
                }
            }
        }
        
        // Second pass: Add mesh objects
        for (auto& node : allNodes) {
            if (node && node->getComponent<MeshRenderer>()) {
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                // Get material color if available
                auto meshRenderer = node->getComponent<MeshRenderer>();
                glm::vec3 materialColor = glm::vec3(1.0f, 0.5f, 0.2f); // Default orange
                if (meshRenderer && meshRenderer->getMaterial()) {
                    materialColor = meshRenderer->getMaterial()->getColor();
                }
                
                // Determine the mesh type and create appropriate code
                auto mesh = meshRenderer->getMesh();
                std::string meshType = "Cube"; // Default
                std::string meshCreationCode = "Mesh::createCube()";
                
                if (mesh) {
                    // Use the stored mesh type for reliable identification
                    switch (mesh->getMeshType()) {
                        case MeshType::QUAD:
                            meshType = "Quad";
                            meshCreationCode = "Mesh::createQuad()";
                            break;
                        case MeshType::PLANE:
                            meshType = "Plane";
                            meshCreationCode = "Mesh::createPlane(1.0f, 1.0f, 1)";
                            break;
                        case MeshType::CUBE:
                            meshType = "Cube";
                            meshCreationCode = "Mesh::createCube()";
                            break;
                        case MeshType::SPHERE:
                            meshType = "Sphere";
                            meshCreationCode = "Mesh::createSphere(32, 16)";
                            break;
                        case MeshType::CAPSULE:
                            meshType = "Capsule";
                            meshCreationCode = "Mesh::createCapsule(0.5f, 0.5f)";
                            break;
                        case MeshType::CYLINDER:
                            meshType = "Cylinder";
                            meshCreationCode = "Mesh::createCylinder(0.5f, 1.0f)";
                            break;
                        default:
                            meshType = "Cube";
                            meshCreationCode = "Mesh::createCube()";
                            break;
                    }
                }
                
                std::string nodeName = meshType + "Node" + std::to_string(shapeCounter);
                nodeNameMap[node.get()] = nodeName;
                
                content += "    // Add a " + meshType + "\n";
                content += "    auto " + nodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    auto meshRenderer" + std::to_string(shapeCounter) + " = " + nodeName + "->addComponent<MeshRenderer>();\n";
                content += "    meshRenderer" + std::to_string(shapeCounter) + "->setMesh(" + meshCreationCode + ");\n";
                content += "    auto material" + std::to_string(shapeCounter) + " = std::make_shared<Material>();\n";
                content += "    material" + std::to_string(shapeCounter) + "->setColor(glm::vec3(" + 
                          std::to_string(materialColor.x) + "f, " + std::to_string(materialColor.y) + "f, " + std::to_string(materialColor.z) + "f));\n";
                
                // Add texture loading based on actual material assignments
                auto material = meshRenderer->getMaterial();
                if (material) {
                    content += "    // Load textures for material\n";
                    
                    // Use the actual texture paths assigned to this material
                    std::string diffusePath = material->getDiffuseTexturePath();
                    std::string normalPath = material->getNormalTexturePath();
                    std::string armPath = material->getARMTexturePath();
                    
                    if (!diffusePath.empty()) {
                        content += "    auto diffuseTexture" + std::to_string(shapeCounter) + " = textureManager.getTexture(\"" + diffusePath + "\");\n";
                        content += "    if (diffuseTexture" + std::to_string(shapeCounter) + ") material" + std::to_string(shapeCounter) + "->setDiffuseTexture(diffuseTexture" + std::to_string(shapeCounter) + ");\n";
                    }
                    
                    if (!normalPath.empty()) {
                        content += "    auto normalTexture" + std::to_string(shapeCounter) + " = textureManager.getTexture(\"" + normalPath + "\");\n";
                        content += "    if (normalTexture" + std::to_string(shapeCounter) + ") material" + std::to_string(shapeCounter) + "->setNormalTexture(normalTexture" + std::to_string(shapeCounter) + ");\n";
                    }
                    
                    if (!armPath.empty()) {
                        content += "    auto armTexture" + std::to_string(shapeCounter) + " = textureManager.getTexture(\"" + armPath + "\");\n";
                        content += "    if (armTexture" + std::to_string(shapeCounter) + ") material" + std::to_string(shapeCounter) + "->setARMTexture(armTexture" + std::to_string(shapeCounter) + ");\n";
                    }
                }
                
                content += "    meshRenderer" + std::to_string(shapeCounter) + "->setMaterial(material" + std::to_string(shapeCounter) + ");\n";
                content += "    " + nodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + nodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + nodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                content += generateVisibilityCode(nodeName, node);
                
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + nodeName + ");\n\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                }
                
                shapeCounter++;
            }
        }
        
        // Model pass: Add model objects
        int modelCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<ModelRenderer>()) {
                auto modelRenderer = node->getComponent<ModelRenderer>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string modelNodeName = "modelNode" + std::to_string(modelCounter);
                nodeNameMap[node.get()] = modelNodeName;
                
                content += "    // Add a model\n";
                content += "    auto " + modelNodeName + " = gameScene->createNode(\"Model " + std::to_string(modelCounter) + "\");\n";
                content += "    auto modelRenderer" + std::to_string(modelCounter) + " = " + modelNodeName + "->addComponent<ModelRenderer>();\n";
                std::string modelPath = modelRenderer->getModelPath();
                std::string relativePath = convertToLinuxPath(modelPath);
                content += "    modelRenderer" + std::to_string(modelCounter) + "->loadModel(\"" + relativePath + "\");\n";
                
                content += "    " + modelNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + modelNodeName + "->getTransform().setRotation(glm::quat(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f)));\n";
                content += "    " + modelNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                // Add to parent or root based on hierarchy
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    // Find the parent's generated name
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + modelNodeName + ");\n\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + modelNodeName + ");\n\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + modelNodeName + ");\n\n";
                }
                
                modelCounter++;
            }
        }
        
        // Third pass: Add lights
        for (auto& node : allNodes) {
            if (node && node->getComponent<LightComponent>()) {
                auto lightComponent = node->getComponent<LightComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                
                std::string lightNodeName = "lightNode" + std::to_string(lightCounter);
                nodeNameMap[node.get()] = lightNodeName;
                
                content += "    // Add a light\n";
                content += "    auto " + lightNodeName + " = gameScene->createNode(\"Light " + std::to_string(lightCounter) + "\");\n";
                content += "    auto lightComponent" + std::to_string(lightCounter) + " = " + lightNodeName + "->addComponent<LightComponent>();\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setType(LightType::" + 
                          (lightComponent->getType() == LightType::POINT ? "POINT" : 
                           lightComponent->getType() == LightType::DIRECTIONAL ? "DIRECTIONAL" : "SPOT") + ");\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setColor(glm::vec3(" + 
                          std::to_string(lightComponent->getColor().x) + "f, " + 
                          std::to_string(lightComponent->getColor().y) + "f, " + 
                          std::to_string(lightComponent->getColor().z) + "f));\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setIntensity(" + 
                          std::to_string(lightComponent->getIntensity()) + "f);\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setRange(" + 
                          std::to_string(lightComponent->getRange()) + "f);\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setShowGizmo(false);\n";
                content += "    " + lightNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + lightNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                
                // Add to parent or root based on hierarchy
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    // Find the parent's generated name
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + lightNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + lightNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + lightNodeName + ");\n";
                }
                
                content += "    lightComponent" + std::to_string(lightCounter) + "->start();\n\n";
                
                lightCounter++;
            }
        }
        
        // Fourth pass: Add physics components
        for (auto& node : allNodes) {
            if (node && node->getComponent<PhysicsComponent>()) {
                auto physicsComponent = node->getComponent<PhysicsComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string physicsNodeName = "physicsNode" + std::to_string(physicsCounter);
                nodeNameMap[node.get()] = physicsNodeName;
                
                content += "    // Add a physics collision node\n";
                content += "    auto " + physicsNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    " + physicsNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + physicsNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + physicsNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                // Add PhysicsComponent with all its properties
                content += "    auto physicsComponent" + std::to_string(physicsCounter) + " = " + physicsNodeName + "->addComponent<PhysicsComponent>();\n";
                
                // Set collision shape type
                auto collisionShapeType = physicsComponent->getCollisionShapeType();
                switch (collisionShapeType) {
                    case CollisionShapeType::BOX:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::BOX);\n";
                        break;
                    case CollisionShapeType::SPHERE:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::SPHERE);\n";
                        break;
                    case CollisionShapeType::CAPSULE:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::CAPSULE);\n";
                        break;
                    case CollisionShapeType::CYLINDER:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::CYLINDER);\n";
                        break;
                    case CollisionShapeType::PLANE:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::PLANE);\n";
                        break;
                }
                
                // Set body type
                auto bodyType = physicsComponent->getBodyType();
                switch (bodyType) {
                    case PhysicsBodyType::STATIC:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setBodyType(PhysicsBodyType::STATIC);\n";
                        break;
                    case PhysicsBodyType::DYNAMIC:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setBodyType(PhysicsBodyType::DYNAMIC);\n";
                        break;
                    case PhysicsBodyType::KINEMATIC:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setBodyType(PhysicsBodyType::KINEMATIC);\n";
                        break;
                }
                
                // Set physics properties
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setMass(" + 
                          std::to_string(physicsComponent->getMass()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setFriction(" + 
                          std::to_string(physicsComponent->getFriction()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setRestitution(" + 
                          std::to_string(physicsComponent->getRestitution()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setLinearDamping(" + 
                          std::to_string(physicsComponent->getLinearDamping()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setAngularDamping(" + 
                          std::to_string(physicsComponent->getAngularDamping()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setShowCollisionShape(true);\n";
                
                // Add to parent or root based on hierarchy
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    // Find the parent's generated name
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + physicsNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + physicsNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + physicsNodeName + ");\n";
                }
                
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->start();\n\n";
                
                physicsCounter++;
            }
        }
        
        // Camera pass: Add camera after physics so we can add it to parent if needed
        auto cameraNode = scene->getActiveCamera();
        if (cameraNode) {
            auto& transform = cameraNode->getTransform();
            auto pos = transform.getPosition();
            auto rot = transform.getEulerAngles();
            auto cameraComponent = cameraNode->getComponent<CameraComponent>();
            
            content += "    // Create Main Camera as a child of Player (so it moves with PlayerRoot -> Player)\n";
            content += "    // Scene structure: PlayerRoot (empty, move this) -> Player (PhysicsComponent) -> Main Camera\n";
            content += "    auto cameraNode = gameScene->createNode(\"Main Camera\");\n";
            content += "    auto cameraComponent = cameraNode->addComponent<CameraComponent>();\n";
            
            // Set camera settings if available
            if (cameraComponent) {
                content += "    cameraComponent->setFOV(" + std::to_string(cameraComponent->getFOV()) + "f);\n";
                content += "    cameraComponent->setNearPlane(" + std::to_string(cameraComponent->getNearPlane()) + "f);\n";
                content += "    cameraComponent->setFarPlane(" + std::to_string(cameraComponent->getFarPlane()) + "f);\n";
                content += "    // Camera controls are now handled by Lua scripts only, not C++ code\n";
                content += "    // cameraComponent->enableControls(false);  // Disabled - scripts control movement\n";
            }
            
            content += "    cameraNode->getTransform().setPosition(glm::vec3(" + 
                      std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));  // Local position relative to Player\n";
            content += "    cameraNode->getTransform().setEulerAngles(glm::vec3(" + 
                      std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
            
            // Add to parent if it exists, otherwise add to root
            if (cameraNode->getParent() && cameraNode->getParent() != scene->getRootNode().get()) {
                // Find the parent's generated name
                auto parentIt = nodeNameMap.find(cameraNode->getParent());
                if (parentIt != nodeNameMap.end()) {
                    content += "    " + parentIt->second + "->addChild(cameraNode);  // Add as child of " + cameraNode->getParent()->getName() + ", not Root!\n";
                } else {
                    // Parent not found in map, add to root
                    content += "    gameScene->getRootNode()->addChild(cameraNode);\n";
                }
            } else {
                // No parent or parent is root, add to root
                content += "    gameScene->getRootNode()->addChild(cameraNode);\n";
            }
            
            content += "    gameScene->setActiveCamera(cameraNode);\n\n";
        }
        
        // Fourth pass: Add text components
        int textCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<TextComponent>()) {
                auto textComponent = node->getComponent<TextComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string textNodeName = "textNode" + std::to_string(textCounter);
                nodeNameMap[node.get()] = textNodeName;
                
                content += "    // Add a text component\n";
                content += "    auto " + textNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    " + textNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + textNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + textNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                content += generateVisibilityCode(textNodeName, node);
                
                // Add TextComponent with all its properties
                content += "    auto textComponent" + std::to_string(textCounter) + " = " + textNodeName + "->addComponent<TextComponent>();\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setText(\"" + escapeStringForCpp(textComponent->getText()) + "\");\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setFontPath(\"" + textComponent->getFontPath() + "\");\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setFontSize(" + std::to_string(textComponent->getFontSize()) + "f);\n";
                
                auto color = textComponent->getColor();
                content += "    textComponent" + std::to_string(textCounter) + "->setColor(glm::vec4(" + 
                          std::to_string(color.r) + "f, " + std::to_string(color.g) + "f, " + 
                          std::to_string(color.b) + "f, " + std::to_string(color.a) + "f));\n";
                
                // Set render mode
                std::string renderMode = "WORLD_SPACE";
                switch (textComponent->getRenderMode()) {
                    case TextRenderMode::WORLD_SPACE: renderMode = "WORLD_SPACE"; break;
                    case TextRenderMode::SCREEN_SPACE: renderMode = "SCREEN_SPACE"; break;
                }
                content += "    textComponent" + std::to_string(textCounter) + "->setRenderMode(TextRenderMode::" + renderMode + ");\n";
                
                // Set alignment
                std::string alignment = "LEFT";
                switch (textComponent->getAlignment()) {
                    case TextAlignment::LEFT: alignment = "LEFT"; break;
                    case TextAlignment::CENTER: alignment = "CENTER"; break;
                    case TextAlignment::RIGHT: alignment = "RIGHT"; break;
                }
                content += "    textComponent" + std::to_string(textCounter) + "->setAlignment(TextAlignment::" + alignment + ");\n";
                
                content += "    textComponent" + std::to_string(textCounter) + "->setScale(" + std::to_string(textComponent->getScale()) + "f);\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setLineSpacing(" + std::to_string(textComponent->getLineSpacing()) + "f);\n";
                
                // Add to parent or root
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + textNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + textNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + textNodeName + ");\n";
                }
                
                content += "    textComponent" + std::to_string(textCounter) + "->start();\n\n";
                
                textCounter++;
            }
        }
        
        // Sixth pass: Add area3D components
        int area3DCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<Area3DComponent>()) {
                auto area3DComponent = node->getComponent<Area3DComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string area3DNodeName = "area3DNode" + std::to_string(area3DCounter);
                nodeNameMap[node.get()] = area3DNodeName;
                
                content += "    // Add an Area3D component\n";
                content += "    auto " + area3DNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    " + area3DNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + area3DNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + area3DNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                // Add Area3DComponent with all its properties
                content += "    auto area3DComponent" + std::to_string(area3DCounter) + " = " + area3DNodeName + "->addComponent<Area3DComponent>();\n";
                
                // Set shape type
                auto shapeType = area3DComponent->getShape();
                switch (shapeType) {
                    case Area3DShape::BOX:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::BOX);\n";
                        break;
                    case Area3DShape::SPHERE:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::SPHERE);\n";
                        break;
                    case Area3DShape::CAPSULE:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::CAPSULE);\n";
                        break;
                    case Area3DShape::CYLINDER:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::CYLINDER);\n";
                        break;
                    case Area3DShape::PLANE:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::PLANE);\n";
                        break;
                }
                
                // Set dimensions/radius/height based on shape
                if (shapeType == Area3DShape::BOX) {
                    auto dims = area3DComponent->getDimensions();
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setDimensions(glm::vec3(" + 
                              std::to_string(dims.x) + "f, " + std::to_string(dims.y) + "f, " + std::to_string(dims.z) + "f));\n";
                } else if (shapeType == Area3DShape::SPHERE) {
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setRadius(" + 
                              std::to_string(area3DComponent->getRadius()) + "f);\n";
                } else if (shapeType == Area3DShape::CAPSULE || shapeType == Area3DShape::CYLINDER) {
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setRadius(" + 
                              std::to_string(area3DComponent->getRadius()) + "f);\n";
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setHeight(" + 
                              std::to_string(area3DComponent->getHeight()) + "f);\n";
                }
                
                // Set group if specified
                if (area3DComponent->hasGroup()) {
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setGroup(\"" + area3DComponent->getGroup() + "\");\n";
                }
                
                // Set monitor mode
                content += "    area3DComponent" + std::to_string(area3DCounter) + "->setMonitorMode(" + 
                          (area3DComponent->getMonitorMode() ? "true" : "false") + ");\n";
                
                // Add to parent or root
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + area3DNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + area3DNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + area3DNodeName + ");\n";
                }
                
                content += "    area3DComponent" + std::to_string(area3DCounter) + "->start();\n\n";
                
                area3DCounter++;
            }
        }
        
        // Fifth pass: Add script components
        int scriptCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<ScriptComponent>()) {
                auto scriptComponent = node->getComponent<ScriptComponent>();
                
                std::string scriptNodeName = "scriptNode" + std::to_string(scriptCounter);
                nodeNameMap[node.get()] = scriptNodeName;
                
                content += "    // Add a script component\n";
                content += "    auto " + scriptNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                
                // Add ScriptComponent with script path
                content += "    auto scriptComponent" + std::to_string(scriptCounter) + " = " + scriptNodeName + "->addComponent<ScriptComponent>();\n";
                content += "    scriptComponent" + std::to_string(scriptCounter) + "->loadScript(\"" + scriptComponent->getScriptPath() + "\");\n";
                
                // Set pause exempt if enabled
                if (scriptComponent->isPauseExempt()) {
                    content += "    scriptComponent" + std::to_string(scriptCounter) + "->setPauseExempt(true);\n";
                }
                
                // Add to root
                content += "    gameScene->getRootNode()->addChild(" + scriptNodeName + ");\n";
                content += "    scriptComponent" + std::to_string(scriptCounter) + "->start();\n\n";
                
                scriptCounter++;
            }
        }
        
        std::string outputMessage = "    std::cout << \"Game scene loaded with camera, " + 
                                    std::to_string(shapeCounter) + " shape(s), " + 
                                    std::to_string(physicsCounter) + " physics component(s), " + 
                                    std::to_string(textCounter - 1) + " text component(s), " + 
                                    std::to_string(scriptCounter - 1) + " script component(s), " + 
                                    std::to_string(area3DCounter - 1) + " area3D component(s), and " + 
                                    std::to_string(lightCounter) + " light(s)!\" << std::endl;\n";
        
        content += outputMessage;
    }
    
    content += R"(
    // Load the game scene
    sceneManager.loadScene(gameScene);
    
    std::cout << "Use WASD to move, mouse to look around" << std::endl;
    std::cout << "Physics simulation is now active - objects will fall and collide!" << std::endl;
    
    // Run the game
    engine.run();
    
    return 0;
}

#endif // LINUX_BUILD
)";
    
    return content;
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
std::string SceneSerializer::generateVitaMainContent(std::shared_ptr<Scene> scene, const std::vector<std::string>& discoveredTextures) {
    if (!scene) return "";
    
    std::string content = R"(
// Vita main file - runs the same scene as Linux game

#include "../game_engine/include/Core/Engine.h"
#include "../game_engine/include/Scene/Scene.h"
#include "../game_engine/include/Scene/SceneNode.h"
#include "../game_engine/include/Components/CameraComponent.h"
#include "../game_engine/include/Components/MeshRenderer.h"
#include "../game_engine/include/Components/ModelRenderer.h"
#include "../game_engine/include/Components/LightComponent.h"
#include "../game_engine/include/Components/PhysicsComponent.h"
#include "../game_engine/include/Components/TextComponent.h"
#include "../game_engine/include/Components/ScriptComponent.h"
#include "../game_engine/include/Components/Area3DComponent.h"
#include "../game_engine/include/Rendering/Mesh.h"
#include "../game_engine/include/Rendering/Material.h"
#include "../game_engine/include/Rendering/TextureManager.h"
#include <vitasdk.h>
#include <vitaGL.h>

using namespace GameEngine;

int main() {
    // Initialize VitaGL first (CRITICAL for Vita builds)
    vglInit(0);

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    
    // Create engine instance
    Engine engine;
    
    // Initialize in game mode
    if (!engine.initialize()) {
        // On Vita, we can't use std::cerr, so just return error
        return -1;
    }
    
    // Enable editor mode for keyboard and mouse input
#ifndef VITA_BUILD
    engine.getInputManager().setEditorMode(true);
#endif
    
    // Initialize texture discovery
    auto& textureManager = TextureManager::getInstance();
    textureManager.discoverAllTextures("assets/textures");
    
    // Create the same scene as the editor (camera + shapes)
    auto& sceneManager = engine.getSceneManager();
    auto gameScene = sceneManager.createScene("Vita Game Scene");
    
)";
    
    // Get the current scene from the editor
    if (scene) {
        // Add all mesh objects (recursively process all mesh nodes in the hierarchy)
        auto allNodes = getAllSceneNodesFromScene(scene);
        std::map<SceneNode*, std::string> nodeNameMap; // Map nodes to their generated names
        int shapeCounter = 0; // Counter for all mesh objects (shapes)
        
        // Initialize counters for final output
        int physicsCounter = 0;
        int lightCounter = 0;
        
        // Sort nodes by depth so parents are processed before children
        std::sort(allNodes.begin(), allNodes.end(), [](const std::shared_ptr<SceneNode>& a, const std::shared_ptr<SceneNode>& b) {
            if (!a || !b) return false;
            
            // Calculate depth of each node
            int depthA = 0, depthB = 0;
            SceneNode* currentA = a->getParent();
            SceneNode* currentB = b->getParent();
            
            while (currentA) { depthA++; currentA = currentA->getParent(); }
            while (currentB) { depthB++; currentB = currentB->getParent(); }
            
            return depthA < depthB; // Process parents first
        });
        
        // First pass: Create all nodes (including empty ones) to establish the hierarchy
        for (auto& node : allNodes) {
            if (node && node != scene->getRootNode()) {
                std::string nodeName;
                
                // Determine node type and name
                if (node->getComponent<MeshRenderer>()) {
                    // This will be handled in the second pass
                    continue;
                } else if (node->getComponent<ModelRenderer>()) {
                    // This will be handled in the model pass
                    continue;
                } else if (node->getComponent<LightComponent>()) {
                    // This will be handled in the third pass
                    continue;
                } else if (node->getComponent<CameraComponent>()) {
                    // Camera is handled separately
                    continue;
                } else if (node->getComponent<PhysicsComponent>()) {
                    // Physics component is handled in a separate pass
                    continue;
                } else if (node->getComponent<TextComponent>()) {
                    // Text component is handled in a separate pass
                    continue;
                } else if (node->getComponent<Area3DComponent>()) {
                    // Area3D component is handled in a separate pass
                    continue;
                } else if (node->getComponent<ScriptComponent>()) {
                    // Script component is handled in a separate pass
                    continue;
                } else {
                    // This is an empty node - create it
                    nodeName = "emptyNode" + std::to_string(nodeNameMap.size());
                    nodeNameMap[node.get()] = nodeName;
                    
                    auto& transform = node->getTransform();
                    auto pos = transform.getPosition();
                    auto rot = transform.getEulerAngles();
                    auto scale = transform.getScale();
                    
                    content += "    // Add an empty node\n";
                    content += "    auto " + nodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                    content += "    " + nodeName + "->getTransform().setPosition(glm::vec3(" + 
                              std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                    content += "    " + nodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                              std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                    content += "    " + nodeName + "->getTransform().setScale(glm::vec3(" + 
                              std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                    
                    content += generateVisibilityCode(nodeName, node);
                    
                    if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                        auto parentIt = nodeNameMap.find(node->getParent());
                        if (parentIt != nodeNameMap.end()) {
                            content += "    " + parentIt->second + "->addChild(" + nodeName + ");\n\n";
                        } else {
                            content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                        }
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                    }
                }
            }
        }
        
        // Second pass: Add mesh objects
        for (auto& node : allNodes) {
            if (node && node->getComponent<MeshRenderer>()) {
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                // Get material color if available
                auto meshRenderer = node->getComponent<MeshRenderer>();
                glm::vec3 materialColor = glm::vec3(1.0f, 0.5f, 0.2f); // Default orange
                if (meshRenderer && meshRenderer->getMaterial()) {
                    materialColor = meshRenderer->getMaterial()->getColor();
                }
                
                // Determine the mesh type and create appropriate code
                auto mesh = meshRenderer->getMesh();
                std::string meshType = "Cube"; // Default
                std::string meshCreationCode = "Mesh::createCube()";
                
                if (mesh) {
                    // Use the stored mesh type for reliable identification
                    switch (mesh->getMeshType()) {
                        case MeshType::QUAD:
                            meshType = "Quad";
                            meshCreationCode = "Mesh::createQuad()";
                            break;
                        case MeshType::PLANE:
                            meshType = "Plane";
                            meshCreationCode = "Mesh::createPlane(1.0f, 1.0f, 1)";
                            break;
                        case MeshType::CUBE:
                            meshType = "Cube";
                            meshCreationCode = "Mesh::createCube()";
                            break;
                        case MeshType::SPHERE:
                            meshType = "Sphere";
                            meshCreationCode = "Mesh::createSphere(32, 16)";
                            break;
                        case MeshType::CAPSULE:
                            meshType = "Capsule";
                            meshCreationCode = "Mesh::createCapsule(0.5f, 0.5f)";
                            break;
                        case MeshType::CYLINDER:
                            meshType = "Cylinder";
                            meshCreationCode = "Mesh::createCylinder(0.5f, 1.0f)";
                            break;
                        default:
                            meshType = "Cube";
                            meshCreationCode = "Mesh::createCube()";
                            break;
                    }
                }
                
                std::string nodeName = meshType + "Node" + std::to_string(shapeCounter);
                nodeNameMap[node.get()] = nodeName;
                
                content += "    // Add a " + meshType + "\n";
                content += "    auto " + nodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    auto meshRenderer" + std::to_string(shapeCounter) + " = " + nodeName + "->addComponent<MeshRenderer>();\n";
                content += "    meshRenderer" + std::to_string(shapeCounter) + "->setMesh(" + meshCreationCode + ");\n";
                content += "    auto material" + std::to_string(shapeCounter) + " = std::make_shared<Material>();\n";
                content += "    material" + std::to_string(shapeCounter) + "->setColor(glm::vec3(" + 
                          std::to_string(materialColor.x) + "f, " + std::to_string(materialColor.y) + "f, " + std::to_string(materialColor.z) + "f));\n";
                
                // Add texture loading based on actual material assignments
                auto material = meshRenderer->getMaterial();
                if (material) {
                    content += "    // Load textures for material\n";
                    
                    // Use the actual texture paths assigned to this material
                    std::string diffusePath = material->getDiffuseTexturePath();
                    std::string normalPath = material->getNormalTexturePath();
                    std::string armPath = material->getARMTexturePath();
                    
                    if (!diffusePath.empty()) {
                        content += "    auto diffuseTexture" + std::to_string(shapeCounter) + " = textureManager.getTexture(\"" + diffusePath + "\");\n";
                        content += "    if (diffuseTexture" + std::to_string(shapeCounter) + ") material" + std::to_string(shapeCounter) + "->setDiffuseTexture(diffuseTexture" + std::to_string(shapeCounter) + ");\n";
                    }
                    
                    if (!normalPath.empty()) {
                        content += "    auto normalTexture" + std::to_string(shapeCounter) + " = textureManager.getTexture(\"" + normalPath + "\");\n";
                        content += "    if (normalTexture" + std::to_string(shapeCounter) + ") material" + std::to_string(shapeCounter) + "->setNormalTexture(normalTexture" + std::to_string(shapeCounter) + ");\n";
                    }
                    
                    if (!armPath.empty()) {
                        content += "    auto armTexture" + std::to_string(shapeCounter) + " = textureManager.getTexture(\"" + armPath + "\");\n";
                        content += "    if (armTexture" + std::to_string(shapeCounter) + ") material" + std::to_string(shapeCounter) + "->setARMTexture(armTexture" + std::to_string(shapeCounter) + ");\n";
                    }
                }
                
                content += "    meshRenderer" + std::to_string(shapeCounter) + "->setMaterial(material" + std::to_string(shapeCounter) + ");\n";
                content += "    " + nodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + nodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + nodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                content += generateVisibilityCode(nodeName, node);
                
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + nodeName + ");\n\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + nodeName + ");\n\n";
                }
                
                shapeCounter++;
            }
        }
        
        // Model pass: Add model objects
        int modelCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<ModelRenderer>()) {
                auto modelRenderer = node->getComponent<ModelRenderer>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string modelNodeName = "modelNode" + std::to_string(modelCounter);
                nodeNameMap[node.get()] = modelNodeName;
                
                content += "    // Add a model\n";
                content += "    auto " + modelNodeName + " = gameScene->createNode(\"Model " + std::to_string(modelCounter) + "\");\n";
                content += "    auto modelRenderer" + std::to_string(modelCounter) + " = " + modelNodeName + "->addComponent<ModelRenderer>();\n";
                // Convert absolute path to relative path for Vita
                std::string modelPath = modelRenderer->getModelPath();
                std::string relativePath = convertToVitaPath(modelPath);
                content += "    modelRenderer" + std::to_string(modelCounter) + "->loadModel(\"" + relativePath + "\");\n";
                
                content += "    " + modelNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + modelNodeName + "->getTransform().setRotation(glm::quat(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f)));\n";
                content += "    " + modelNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                // Add to parent or root based on hierarchy
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    // Find the parent's generated name
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + modelNodeName + ");\n\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + modelNodeName + ");\n\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + modelNodeName + ");\n\n";
                }
                
                modelCounter++;
            }
        }
        
        // Third pass: Add lights
        for (auto& node : allNodes) {
            if (node && node->getComponent<LightComponent>()) {
                auto lightComponent = node->getComponent<LightComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                
                std::string lightNodeName = "lightNode" + std::to_string(lightCounter);
                nodeNameMap[node.get()] = lightNodeName;
                
                content += "    // Add a light\n";
                content += "    auto " + lightNodeName + " = gameScene->createNode(\"Light " + std::to_string(lightCounter) + "\");\n";
                content += "    auto lightComponent" + std::to_string(lightCounter) + " = " + lightNodeName + "->addComponent<LightComponent>();\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setType(LightType::" + 
                          (lightComponent->getType() == LightType::POINT ? "POINT" : 
                           lightComponent->getType() == LightType::DIRECTIONAL ? "DIRECTIONAL" : "SPOT") + ");\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setColor(glm::vec3(" + 
                          std::to_string(lightComponent->getColor().x) + "f, " + 
                          std::to_string(lightComponent->getColor().y) + "f, " + 
                          std::to_string(lightComponent->getColor().z) + "f));\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setIntensity(" + 
                          std::to_string(lightComponent->getIntensity()) + "f);\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setRange(" + 
                          std::to_string(lightComponent->getRange()) + "f);\n";
                content += "    lightComponent" + std::to_string(lightCounter) + "->setShowGizmo(false);\n";
                content += "    " + lightNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + lightNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                
                // Add to parent or root based on hierarchy
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    // Find the parent's generated name
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + lightNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + lightNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + lightNodeName + ");\n";
                }
                
                content += "    lightComponent" + std::to_string(lightCounter) + "->start();\n\n";
                
                lightCounter++;
            }
        }
        
        // Fourth pass: Add physics components
        for (auto& node : allNodes) {
            if (node && node->getComponent<PhysicsComponent>()) {
                auto physicsComponent = node->getComponent<PhysicsComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string physicsNodeName = "physicsNode" + std::to_string(physicsCounter);
                nodeNameMap[node.get()] = physicsNodeName;
                
                content += "    // Add a physics collision node\n";
                content += "    auto " + physicsNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    " + physicsNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + physicsNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + physicsNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                // Add PhysicsComponent with all its properties
                content += "    auto physicsComponent" + std::to_string(physicsCounter) + " = " + physicsNodeName + "->addComponent<PhysicsComponent>();\n";
                
                // Set collision shape type
                auto collisionShapeType = physicsComponent->getCollisionShapeType();
                switch (collisionShapeType) {
                    case CollisionShapeType::BOX:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::BOX);\n";
                        break;
                    case CollisionShapeType::SPHERE:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::SPHERE);\n";
                        break;
                    case CollisionShapeType::CAPSULE:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::CAPSULE);\n";
                        break;
                    case CollisionShapeType::CYLINDER:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::CYLINDER);\n";
                        break;
                    case CollisionShapeType::PLANE:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setCollisionShape(CollisionShapeType::PLANE);\n";
                        break;
                }
                
                // Set body type
                auto bodyType = physicsComponent->getBodyType();
                switch (bodyType) {
                    case PhysicsBodyType::STATIC:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setBodyType(PhysicsBodyType::STATIC);\n";
                        break;
                    case PhysicsBodyType::DYNAMIC:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setBodyType(PhysicsBodyType::DYNAMIC);\n";
                        break;
                    case PhysicsBodyType::KINEMATIC:
                        content += "    physicsComponent" + std::to_string(physicsCounter) + "->setBodyType(PhysicsBodyType::KINEMATIC);\n";
                        break;
                }
                
                // Set physics properties
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setMass(" + 
                          std::to_string(physicsComponent->getMass()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setFriction(" + 
                          std::to_string(physicsComponent->getFriction()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setRestitution(" + 
                          std::to_string(physicsComponent->getRestitution()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setLinearDamping(" + 
                          std::to_string(physicsComponent->getLinearDamping()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setAngularDamping(" + 
                          std::to_string(physicsComponent->getAngularDamping()) + "f);\n";
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->setShowCollisionShape(true);\n";
                
                // Add to parent or root based on hierarchy
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    // Find the parent's generated name
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + physicsNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + physicsNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + physicsNodeName + ");\n";
                }
                
                content += "    physicsComponent" + std::to_string(physicsCounter) + "->start();\n\n";
                
                physicsCounter++;
            }
        }
        
        // Camera pass: Add camera after physics so we can add it to parent if needed
        auto cameraNode = scene->getActiveCamera();
        if (cameraNode) {
            auto& transform = cameraNode->getTransform();
            auto pos = transform.getPosition();
            auto rot = transform.getEulerAngles();
            auto cameraComponent = cameraNode->getComponent<CameraComponent>();
            
            content += "    // Create Main Camera as a child of Player (so it moves with PlayerRoot -> Player)\n";
            content += "    // Scene structure: PlayerRoot (empty, move this) -> Player (PhysicsComponent) -> Main Camera\n";
            content += "    auto cameraNode = gameScene->createNode(\"Main Camera\");\n";
            content += "    auto cameraComponent = cameraNode->addComponent<CameraComponent>();\n";
            
            // Set camera settings if available
            if (cameraComponent) {
                content += "    cameraComponent->setFOV(" + std::to_string(cameraComponent->getFOV()) + "f);\n";
                content += "    cameraComponent->setNearPlane(" + std::to_string(cameraComponent->getNearPlane()) + "f);\n";
                content += "    cameraComponent->setFarPlane(" + std::to_string(cameraComponent->getFarPlane()) + "f);\n";
                content += "    // Camera controls are now handled by Lua scripts only, not C++ code\n";
                content += "    // cameraComponent->enableControls(false);  // Disabled - scripts control movement\n";
            }
            
            content += "    cameraNode->getTransform().setPosition(glm::vec3(" + 
                      std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));  // Local position relative to Player\n";
            content += "    cameraNode->getTransform().setEulerAngles(glm::vec3(" + 
                      std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
            
            // Add to parent if it exists, otherwise add to root
            if (cameraNode->getParent() && cameraNode->getParent() != scene->getRootNode().get()) {
                // Find the parent's generated name
                auto parentIt = nodeNameMap.find(cameraNode->getParent());
                if (parentIt != nodeNameMap.end()) {
                    content += "    " + parentIt->second + "->addChild(cameraNode);  // Add as child of " + cameraNode->getParent()->getName() + ", not Root!\n";
                } else {
                    // Parent not found in map, add to root
                    content += "    gameScene->getRootNode()->addChild(cameraNode);\n";
                }
            } else {
                // No parent or parent is root, add to root
                content += "    gameScene->getRootNode()->addChild(cameraNode);\n";
            }
            
            content += "    gameScene->setActiveCamera(cameraNode);\n\n";
        }
        
        // Fourth pass: Add text components
        int textCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<TextComponent>()) {
                auto textComponent = node->getComponent<TextComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string textNodeName = "textNode" + std::to_string(textCounter);
                nodeNameMap[node.get()] = textNodeName;
                
                content += "    // Add a text component\n";
                content += "    auto " + textNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    " + textNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + textNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + textNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                content += generateVisibilityCode(textNodeName, node);
                
                // Add TextComponent with all its properties
                content += "    auto textComponent" + std::to_string(textCounter) + " = " + textNodeName + "->addComponent<TextComponent>();\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setText(\"" + escapeStringForCpp(textComponent->getText()) + "\");\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setFontPath(\"" + textComponent->getFontPath() + "\");\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setFontSize(" + std::to_string(textComponent->getFontSize()) + "f);\n";
                
                auto color = textComponent->getColor();
                content += "    textComponent" + std::to_string(textCounter) + "->setColor(glm::vec4(" + 
                          std::to_string(color.r) + "f, " + std::to_string(color.g) + "f, " + 
                          std::to_string(color.b) + "f, " + std::to_string(color.a) + "f));\n";
                
                // Set render mode
                std::string renderMode = "WORLD_SPACE";
                switch (textComponent->getRenderMode()) {
                    case TextRenderMode::WORLD_SPACE: renderMode = "WORLD_SPACE"; break;
                    case TextRenderMode::SCREEN_SPACE: renderMode = "SCREEN_SPACE"; break;
                }
                content += "    textComponent" + std::to_string(textCounter) + "->setRenderMode(TextRenderMode::" + renderMode + ");\n";
                
                // Set alignment
                std::string alignment = "LEFT";
                switch (textComponent->getAlignment()) {
                    case TextAlignment::LEFT: alignment = "LEFT"; break;
                    case TextAlignment::CENTER: alignment = "CENTER"; break;
                    case TextAlignment::RIGHT: alignment = "RIGHT"; break;
                }
                content += "    textComponent" + std::to_string(textCounter) + "->setAlignment(TextAlignment::" + alignment + ");\n";
                
                content += "    textComponent" + std::to_string(textCounter) + "->setScale(" + std::to_string(textComponent->getScale()) + "f);\n";
                content += "    textComponent" + std::to_string(textCounter) + "->setLineSpacing(" + std::to_string(textComponent->getLineSpacing()) + "f);\n";
                
                // Add to parent or root
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + textNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + textNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + textNodeName + ");\n";
                }
                
                content += "    textComponent" + std::to_string(textCounter) + "->start();\n\n";
                
                textCounter++;
            }
        }
        
        // Sixth pass: Add area3D components
        int area3DCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<Area3DComponent>()) {
                auto area3DComponent = node->getComponent<Area3DComponent>();
                auto& transform = node->getTransform();
                auto pos = transform.getPosition();
                auto rot = transform.getEulerAngles();
                auto scale = transform.getScale();
                
                std::string area3DNodeName = "area3DNode" + std::to_string(area3DCounter);
                nodeNameMap[node.get()] = area3DNodeName;
                
                content += "    // Add an Area3D component\n";
                content += "    auto " + area3DNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                content += "    " + area3DNodeName + "->getTransform().setPosition(glm::vec3(" + 
                          std::to_string(pos.x) + "f, " + std::to_string(pos.y) + "f, " + std::to_string(pos.z) + "f));\n";
                content += "    " + area3DNodeName + "->getTransform().setEulerAngles(glm::vec3(" + 
                          std::to_string(rot.x) + "f, " + std::to_string(rot.y) + "f, " + std::to_string(rot.z) + "f));\n";
                content += "    " + area3DNodeName + "->getTransform().setScale(glm::vec3(" + 
                          std::to_string(scale.x) + "f, " + std::to_string(scale.y) + "f, " + std::to_string(scale.z) + "f));\n";
                
                // Add Area3DComponent with all its properties
                content += "    auto area3DComponent" + std::to_string(area3DCounter) + " = " + area3DNodeName + "->addComponent<Area3DComponent>();\n";
                
                // Set shape type
                auto shapeType = area3DComponent->getShape();
                switch (shapeType) {
                    case Area3DShape::BOX:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::BOX);\n";
                        break;
                    case Area3DShape::SPHERE:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::SPHERE);\n";
                        break;
                    case Area3DShape::CAPSULE:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::CAPSULE);\n";
                        break;
                    case Area3DShape::CYLINDER:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::CYLINDER);\n";
                        break;
                    case Area3DShape::PLANE:
                        content += "    area3DComponent" + std::to_string(area3DCounter) + "->setShape(Area3DShape::PLANE);\n";
                        break;
                }
                
                // Set dimensions/radius/height based on shape
                if (shapeType == Area3DShape::BOX) {
                    auto dims = area3DComponent->getDimensions();
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setDimensions(glm::vec3(" + 
                              std::to_string(dims.x) + "f, " + std::to_string(dims.y) + "f, " + std::to_string(dims.z) + "f));\n";
                } else if (shapeType == Area3DShape::SPHERE) {
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setRadius(" + 
                              std::to_string(area3DComponent->getRadius()) + "f);\n";
                } else if (shapeType == Area3DShape::CAPSULE || shapeType == Area3DShape::CYLINDER) {
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setRadius(" + 
                              std::to_string(area3DComponent->getRadius()) + "f);\n";
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setHeight(" + 
                              std::to_string(area3DComponent->getHeight()) + "f);\n";
                }
                
                // Set group if specified
                if (area3DComponent->hasGroup()) {
                    content += "    area3DComponent" + std::to_string(area3DCounter) + "->setGroup(\"" + area3DComponent->getGroup() + "\");\n";
                }
                
                // Set monitor mode
                content += "    area3DComponent" + std::to_string(area3DCounter) + "->setMonitorMode(" + 
                          (area3DComponent->getMonitorMode() ? "true" : "false") + ");\n";
                
                // Add to parent or root
                if (node->getParent() && node->getParent() != scene->getRootNode().get()) {
                    auto parentIt = nodeNameMap.find(node->getParent());
                    if (parentIt != nodeNameMap.end()) {
                        content += "    " + parentIt->second + "->addChild(" + area3DNodeName + ");\n";
                    } else {
                        content += "    gameScene->getRootNode()->addChild(" + area3DNodeName + ");\n";
                    }
                } else {
                    content += "    gameScene->getRootNode()->addChild(" + area3DNodeName + ");\n";
                }
                
                content += "    area3DComponent" + std::to_string(area3DCounter) + "->start();\n\n";
                
                area3DCounter++;
            }
        }
        
        // Fifth pass: Add script components
        int scriptCounter = 1;
        for (auto& node : allNodes) {
            if (node && node->getComponent<ScriptComponent>()) {
                auto scriptComponent = node->getComponent<ScriptComponent>();
                
                std::string scriptNodeName = "scriptNode" + std::to_string(scriptCounter);
                nodeNameMap[node.get()] = scriptNodeName;
                
                content += "    // Add a script component\n";
                content += "    auto " + scriptNodeName + " = gameScene->createNode(\"" + node->getName() + "\");\n";
                
                // Add ScriptComponent with script path
                content += "    auto scriptComponent" + std::to_string(scriptCounter) + " = " + scriptNodeName + "->addComponent<ScriptComponent>();\n";
                content += "    scriptComponent" + std::to_string(scriptCounter) + "->loadScript(\"" + scriptComponent->getScriptPath() + "\");\n";
                
                // Set pause exempt if enabled
                if (scriptComponent->isPauseExempt()) {
                    content += "    scriptComponent" + std::to_string(scriptCounter) + "->setPauseExempt(true);\n";
                }
                
                // Add to root
                content += "    gameScene->getRootNode()->addChild(" + scriptNodeName + ");\n";
                content += "    scriptComponent" + std::to_string(scriptCounter) + "->start();\n\n";
                
                scriptCounter++;
            }
        }
    }
    
    content += R"(
    // Load the game scene
    sceneManager.loadScene(gameScene);
    
    // Run the game
    engine.run();
    
    // Cleanup VitaGL
    vglEnd();
    
    return 0;
}
)";
    
    return content;
}
#endif // LINUX_BUILD

void SceneSerializer::collectAllNodesRecursive(std::shared_ptr<SceneNode> node, std::vector<std::shared_ptr<SceneNode>>& allNodes) {
    if (!node) return;
    
    allNodes.push_back(node);
    
    for (size_t i = 0; i < node->getChildCount(); ++i) {
        collectAllNodesRecursive(node->getChild(i), allNodes);
    }
}

std::vector<std::shared_ptr<SceneNode>> SceneSerializer::getAllSceneNodesFromScene(std::shared_ptr<Scene> scene) {
    std::vector<std::shared_ptr<SceneNode>> allNodes;
    
    if (scene) {
        auto rootNode = scene->getRootNode();
        if (rootNode) {
            collectAllNodesRecursive(rootNode, allNodes);
        }
    }
    
    return allNodes;
}

std::string SceneSerializer::sanitizeNodeName(const std::string& name) {
    std::string sanitized = name;
    
    // Replace spaces and special characters with underscores
    for (char& c : sanitized) {
        if (!isalnum(c) && c != '_') {
            c = '_';
        }
    }
    
    // Ensure it starts with a letter or underscore
    if (!sanitized.empty() && !isalpha(sanitized[0]) && sanitized[0] != '_') {
        sanitized = "Node_" + sanitized;
    }
    
    // Remove consecutive underscores
    size_t pos = 0;
    while ((pos = sanitized.find("__", pos)) != std::string::npos) {
        sanitized.erase(pos, 1);
    }
    
    return sanitized;
}

bool SceneSerializer::saveSceneToFile(std::shared_ptr<Scene> scene, const std::string& filepath) {
    if (!scene) {
#ifdef VITA_BUILD
        printf("Cannot save null scene to file: %s\n", filepath.c_str());
#else
        std::cerr << "Cannot save null scene to file: " << filepath << std::endl;
#endif
        return false;
    }
    
#ifdef VITA_BUILD
    // Vita build: no exception handling
    std::string jsonData = serializeSceneToJson(scene);
    
    // Use Vita file I/O
    SceUID fd = sceIoOpen(filepath.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) {
        printf("Failed to open file for writing: %s (error: 0x%08X)\n", filepath.c_str(), fd);
        return false;
    }
    
    SceSSize bytesWritten = sceIoWrite(fd, jsonData.c_str(), jsonData.size());
    sceIoClose(fd);
    
    if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != jsonData.size()) {
        printf("Failed to write file: %s (error: 0x%08X)\n", filepath.c_str(), bytesWritten);
        return false;
    }
    
    printf("Scene saved successfully to: %s\n", filepath.c_str());
    return true;
#else
    try {
        std::string jsonData = serializeSceneToJson(scene);
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filepath << std::endl;
            return false;
        }
        
        file << jsonData;
        file.close();
        
        std::cout << "Scene saved successfully to: " << filepath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving scene to file: " << e.what() << std::endl;
        return false;
    }
#endif
}

std::shared_ptr<Scene> SceneSerializer::loadSceneFromFile(const std::string& filepath) {
#ifdef VITA_BUILD
    // Vita build: no exception handling
    std::string jsonData;
    
    // Convert filepath to Vita format (app0:/path for VPK files)
    std::string vitaPath = filepath;
    
    // Check if path already has a device prefix (app0:, ux0:, ur0:, etc.)
    if (filepath.find("app0:") == std::string::npos && 
        filepath.find("ux0:") == std::string::npos && 
        filepath.find("ur0:") == std::string::npos &&
        filepath.find("uma0:") == std::string::npos &&
        filepath.find("imc0:") == std::string::npos &&
        filepath.find("xmc0:") == std::string::npos &&
        filepath.find("vs0:") == std::string::npos &&
        filepath.find("vd0:") == std::string::npos) {
        // No device prefix found, prepend app0: for VPK access
        vitaPath = "app0:/" + filepath;
    }
    
    // Use Vita file I/O
    SceUID fd = sceIoOpen(vitaPath.c_str(), SCE_O_RDONLY, 0);
    if (fd < 0) {
        printf("Failed to open file for reading: %s (tried: %s, error: 0x%08X)\n", filepath.c_str(), vitaPath.c_str(), fd);
        return nullptr;
    }
    
    // Get file size
    SceIoStat stat;
    if (sceIoGetstat(vitaPath.c_str(), &stat) < 0) {
        sceIoClose(fd);
        printf("Failed to get file stat: %s (tried: %s)\n", filepath.c_str(), vitaPath.c_str());
        return nullptr;
    }
    
    // Read file content
    std::vector<char> buffer(stat.st_size);
    SceSSize bytesRead = sceIoRead(fd, buffer.data(), stat.st_size);
    sceIoClose(fd);
    
    if (bytesRead < 0) {
        printf("Failed to read file: %s (tried: %s, error: 0x%08X)\n", filepath.c_str(), vitaPath.c_str(), bytesRead);
        return nullptr;
    }
    
    jsonData = std::string(buffer.data(), bytesRead);
    
    if (jsonData.empty()) {
        printf("File is empty: %s (tried: %s)\n", filepath.c_str(), vitaPath.c_str());
        return nullptr;
    }
    
    std::shared_ptr<Scene> scene = deserializeSceneFromJson(jsonData);
    if (scene) {
        printf("Scene loaded successfully from: %s\n", vitaPath.c_str());
    }
    
    return scene;
#else
    try {
        std::string jsonData;
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for reading: " << filepath << std::endl;
            return nullptr;
        }
        
        jsonData = std::string((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
        file.close();
        
        if (jsonData.empty()) {
            std::cerr << "File is empty: " << filepath << std::endl;
            return nullptr;
        }
        
        std::shared_ptr<Scene> scene = deserializeSceneFromJson(jsonData);
        if (scene) {
            std::cout << "Scene loaded successfully from: " << filepath << std::endl;
        }
        
        return scene;
    } catch (const std::exception& e) {
        std::cerr << "Error loading scene from file: " << e.what() << std::endl;
        return nullptr;
    }
#endif
}

std::string SceneSerializer::serializeSceneToJson(std::shared_ptr<Scene> scene) {
    if (!scene) return "{}";
    
    json sceneJson;
    sceneJson["name"] = scene->getName();
    sceneJson["version"] = "1.0";
    
    // Serialize root node
    auto rootNode = scene->getRootNode();
    if (rootNode) {
        sceneJson["rootNode"] = serializeNodeToJson(rootNode);
    }
    
    // Serialize active camera reference
    auto activeCamera = scene->getActiveCamera();
    if (activeCamera) {
        sceneJson["activeCamera"] = activeCamera->getName();
    } else {
        sceneJson["activeCamera"] = nullptr;
    }
    
    return sceneJson.dump(2); // Pretty print with 2 spaces indentation
}

std::shared_ptr<Scene> SceneSerializer::deserializeSceneFromJson(const std::string& jsonData) {
#ifndef VITA_BUILD
    try {
#endif
        json sceneJson = json::parse(jsonData);
        
        std::string sceneName = sceneJson.value("name", "Loaded Scene");
        auto scene = std::make_shared<Scene>(sceneName);
        
        // Clear the default root node's children
        scene->getRootNode()->removeAllChildren();
        
        // Deserialize root node
        if (sceneJson.contains("rootNode")) {
            auto rootNode = deserializeNodeFromJson(sceneJson["rootNode"]);
            if (rootNode) {
                // Replace the scene's root node with the loaded one
                scene->getRootNode()->setName(rootNode->getName());
                scene->getRootNode()->setVisible(rootNode->isVisible());
                scene->getRootNode()->setActive(rootNode->isActive());
                scene->getRootNode()->getTransform() = rootNode->getTransform();
                
                // Copy children - collect them first to avoid issues with changing indices
                std::vector<std::shared_ptr<SceneNode>> childrenToAdd;
                for (size_t i = 0; i < rootNode->getChildCount(); ++i) {
                    auto child = rootNode->getChild(i);
                    if (child) {
                        childrenToAdd.push_back(child);
                    }
                }
                
                // Now add all collected children
                for (auto& child : childrenToAdd) {
                    scene->getRootNode()->addChild(child);
                }
            }
        }
        
        // Set active camera
        if (sceneJson.contains("activeCamera") && !sceneJson["activeCamera"].is_null()) {
            std::string activeCameraName = sceneJson["activeCamera"];
            auto cameraNode = scene->findNode(activeCameraName);
            if (cameraNode) {
                scene->setActiveCamera(cameraNode);
            }
        }
        
#ifdef VITA_BUILD
        printf("Scene loaded successfully from: %s\n", sceneName.c_str());
#else
        std::cout << "Scene loaded successfully from: " << sceneName << std::endl;
#endif
        return scene;
#ifndef VITA_BUILD
    } catch (const json::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return nullptr;
    } catch (const std::exception& e) {
        std::cerr << "Error deserializing scene from JSON: " << e.what() << std::endl;
        return nullptr;
    }
#endif
}

nlohmann::json SceneSerializer::serializeNodeToJson(std::shared_ptr<SceneNode> node) {
    if (!node) return json::object();
    
    json nodeJson;
    nodeJson["name"] = node->getName();
    nodeJson["visible"] = node->isVisible();
    nodeJson["active"] = node->isActive();
    
    // Transform
    auto& transform = node->getTransform();
    auto position = transform.getPosition();
    auto rotation = transform.getEulerAngles();
    auto scale = transform.getScale();
    
    json transformJson;
    transformJson["position"] = {position.x, position.y, position.z};
    transformJson["rotation"] = {rotation.x, rotation.y, rotation.z};
    transformJson["scale"] = {scale.x, scale.y, scale.z};
    nodeJson["transform"] = transformJson;
    
    // Components
    const auto& components = node->getAllComponents();
    if (!components.empty()) {
        json componentsArray = json::array();
        
        for (const auto& component : components) {
            if (component && component->isEnabled()) {
                json componentJson;
                componentJson["type"] = component->getTypeName();
                
                // Serialize specific component data
                if (component->getTypeName() == "CameraComponent") {
                    auto cameraComp = node->getComponent<CameraComponent>();
                    if (cameraComp) {
                        componentJson["fov"] = cameraComp->getFOV();
                        componentJson["nearPlane"] = cameraComp->getNearPlane();
                        componentJson["farPlane"] = cameraComp->getFarPlane();
                        componentJson["active"] = cameraComp->isActive();
                    }
                } else if (component->getTypeName() == "MeshRenderer") {
                    auto meshRenderer = node->getComponent<MeshRenderer>();
                    if (meshRenderer) {
                        auto mesh = meshRenderer->getMesh();
                        if (mesh) {
                            // Store mesh type
                            std::string meshType = "CUBE";
                            switch (mesh->getMeshType()) {
                                case MeshType::QUAD: meshType = "QUAD"; break;
                                case MeshType::PLANE: meshType = "PLANE"; break;
                                case MeshType::CUBE: meshType = "CUBE"; break;
                                case MeshType::SPHERE: meshType = "SPHERE"; break;
                                case MeshType::CAPSULE: meshType = "CAPSULE"; break;
                                case MeshType::CYLINDER: meshType = "CYLINDER"; break;
                                default: meshType = "CUBE"; break;
                            }
                            componentJson["meshType"] = meshType;
                        }
                        
                        auto material = meshRenderer->getMaterial();
                        if (material) {
                            auto color = material->getColor();
                            json materialJson;
                            materialJson["color"] = {color.x, color.y, color.z};
                            
                            // Save texture paths
                            std::string diffusePath = material->getDiffuseTexturePath();
                            std::string normalPath = material->getNormalTexturePath();
                            std::string armPath = material->getARMTexturePath();
                            
                            if (!diffusePath.empty()) {
                                materialJson["diffuseTexture"] = diffusePath;
                            }
                            if (!normalPath.empty()) {
                                materialJson["normalTexture"] = normalPath;
                            }
                            if (!armPath.empty()) {
                                materialJson["armTexture"] = armPath;
                            }
                            
                            componentJson["material"] = materialJson;
                        }
                    }
                } else if (component->getTypeName() == "ModelRenderer") {
                    auto modelRenderer = node->getComponent<ModelRenderer>();
                    if (modelRenderer) {
                        componentJson["modelPath"] = modelRenderer->getModelPath();
                        componentJson["modelName"] = modelRenderer->getModelName();
                        componentJson["isLoaded"] = modelRenderer->isModelLoaded();
                        componentJson["castShadows"] = modelRenderer->getCastShadows();
                        componentJson["receiveShadows"] = modelRenderer->getReceiveShadows();
                    }
                } else if (component->getTypeName() == "LightComponent") {
                    auto lightComp = node->getComponent<LightComponent>();
                    if (lightComp) {
                        auto color = lightComp->getColor();
                        componentJson["color"] = {color.x, color.y, color.z};
                        componentJson["intensity"] = lightComp->getIntensity();
                        componentJson["range"] = lightComp->getRange();
                        componentJson["showGizmo"] = lightComp->getShowGizmo();
                        
                        // Light type
                        std::string lightType = "POINT";
                        switch (lightComp->getType()) {
                            case LightType::POINT: lightType = "POINT"; break;
                            case LightType::DIRECTIONAL: lightType = "DIRECTIONAL"; break;
                            case LightType::SPOT: lightType = "SPOT"; break;
                        }
                        componentJson["lightType"] = lightType;
                        
                        auto direction = lightComp->getDirection();
                        componentJson["direction"] = {direction.x, direction.y, direction.z};
                        componentJson["cutOff"] = lightComp->getCutOff();
                        componentJson["outerCutOff"] = lightComp->getOuterCutOff();
                    }
                } else if (component->getTypeName() == "PhysicsComponent") {
                    auto physicsComp = node->getComponent<PhysicsComponent>();
                    if (physicsComp) {
                        componentJson["mass"] = physicsComp->getMass();
                        componentJson["friction"] = physicsComp->getFriction();
                        componentJson["restitution"] = physicsComp->getRestitution();
                        componentJson["linearDamping"] = physicsComp->getLinearDamping();
                        componentJson["angularDamping"] = physicsComp->getAngularDamping();
                        componentJson["showCollisionShape"] = physicsComp->getShowCollisionShape();
                        
                        // Collision shape type
                        std::string shapeType = "BOX";
                        switch (physicsComp->getCollisionShapeType()) {
                            case CollisionShapeType::BOX: shapeType = "BOX"; break;
                            case CollisionShapeType::SPHERE: shapeType = "SPHERE"; break;
                            case CollisionShapeType::CAPSULE: shapeType = "CAPSULE"; break;
                            case CollisionShapeType::CYLINDER: shapeType = "CYLINDER"; break;
                            case CollisionShapeType::PLANE: shapeType = "PLANE"; break;
                        }
                        componentJson["collisionShapeType"] = shapeType;
                        
                        // Body type
                        std::string bodyType = "STATIC";
                        switch (physicsComp->getBodyType()) {
                            case PhysicsBodyType::STATIC: bodyType = "STATIC"; break;
                            case PhysicsBodyType::DYNAMIC: bodyType = "DYNAMIC"; break;
                            case PhysicsBodyType::KINEMATIC: bodyType = "KINEMATIC"; break;
                        }
                        componentJson["bodyType"] = bodyType;
                    }
                } else if (component->getTypeName() == "TextComponent") {
                    auto textComp = node->getComponent<TextComponent>();
                    if (textComp) {
                        componentJson["text"] = textComp->getText();
                        componentJson["fontPath"] = textComp->getFontPath();
                        componentJson["fontSize"] = textComp->getFontSize();
                        componentJson["color"] = {textComp->getColor().r, textComp->getColor().g, textComp->getColor().b, textComp->getColor().a};
                        
                        // Render mode
                        std::string renderMode = "WORLD_SPACE";
                        switch (textComp->getRenderMode()) {
                            case TextRenderMode::WORLD_SPACE: renderMode = "WORLD_SPACE"; break;
                            case TextRenderMode::SCREEN_SPACE: renderMode = "SCREEN_SPACE"; break;
                        }
                        componentJson["renderMode"] = renderMode;
                        
                        // Alignment
                        std::string alignment = "LEFT";
                        switch (textComp->getAlignment()) {
                            case TextAlignment::LEFT: alignment = "LEFT"; break;
                            case TextAlignment::CENTER: alignment = "CENTER"; break;
                            case TextAlignment::RIGHT: alignment = "RIGHT"; break;
                        }
                        componentJson["alignment"] = alignment;
                        
                        componentJson["scale"] = textComp->getScale();
                        componentJson["lineSpacing"] = textComp->getLineSpacing();
                    }
                } else if (component->getTypeName() == "ScriptComponent") {
                    auto scriptComp = node->getComponent<ScriptComponent>();
                    if (scriptComp) {
                        std::string scriptPath = scriptComp->getScriptPath();
                        if (!scriptPath.empty()) {
                            componentJson["scriptPath"] = scriptPath;
                        }
                        // Save pause exempt setting
                        componentJson["pauseExempt"] = scriptComp->isPauseExempt();
                    }
                } else if (component->getTypeName() == "Area3DComponent") {
                    auto area3DComp = node->getComponent<Area3DComponent>();
                    if (area3DComp) {
                        // Shape type
                        std::string shapeTypeStr = "BOX";
                        switch (area3DComp->getShape()) {
                            case Area3DShape::BOX: shapeTypeStr = "BOX"; break;
                            case Area3DShape::SPHERE: shapeTypeStr = "SPHERE"; break;
                            case Area3DShape::CAPSULE: shapeTypeStr = "CAPSULE"; break;
                            case Area3DShape::CYLINDER: shapeTypeStr = "CYLINDER"; break;
                            case Area3DShape::PLANE: shapeTypeStr = "PLANE"; break;
                        }
                        componentJson["shapeType"] = shapeTypeStr;
                        
                        // Shape properties
                        if (area3DComp->getShape() == Area3DShape::BOX) {
                            auto dims = area3DComp->getDimensions();
                            componentJson["dimensions"] = {dims.x, dims.y, dims.z};
                        } else if (area3DComp->getShape() == Area3DShape::SPHERE) {
                            componentJson["radius"] = area3DComp->getRadius();
                        } else if (area3DComp->getShape() == Area3DShape::CAPSULE || area3DComp->getShape() == Area3DShape::CYLINDER) {
                            componentJson["radius"] = area3DComp->getRadius();
                            componentJson["height"] = area3DComp->getHeight();
                        }
                        
                        // Group
                        if (area3DComp->hasGroup()) {
                            componentJson["group"] = area3DComp->getGroup();
                        }
                        
                        // Monitor mode
                        componentJson["monitorMode"] = area3DComp->getMonitorMode();
                        
                        // Debug
                        componentJson["showDebugShape"] = area3DComp->getShowDebugShape();
                    }
                } else if (component->getTypeName() == "AnimationComponent") {
                    auto animComp = node->getComponent<AnimationComponent>();
                    if (animComp) {
                        auto skeleton = animComp->getSkeleton();
                        if (skeleton) {
                            componentJson["skeletonName"] = skeleton->getName();
                        }
                        
                        auto clip = animComp->getCurrentAnimationClip();
                        if (clip) {
                            componentJson["animationClipName"] = clip->getName();
                        }
                        
                        componentJson["loop"] = animComp->getLoop();
                        componentJson["speed"] = animComp->getSpeed();
                        componentJson["autoPlay"] = animComp->isPlaying();
                        componentJson["enableRootMotion"] = animComp->isRootMotionEnabled();
                    }
                }
                
                componentsArray.push_back(componentJson);
            }
        }
        
        nodeJson["components"] = componentsArray;
    }
    
    // Children
    if (node->getChildCount() > 0) {
        json childrenArray = json::array();
        for (size_t i = 0; i < node->getChildCount(); ++i) {
            auto child = node->getChild(i);
            if (child) {
                childrenArray.push_back(serializeNodeToJson(child));
            }
        }
        nodeJson["children"] = childrenArray;
    }
    
    return nodeJson;
}

std::shared_ptr<SceneNode> SceneSerializer::deserializeNodeFromJson(const json& nodeJson) {
#ifndef VITA_BUILD
    try {
#endif
        std::string name = nodeJson.value("name", "Node");
        auto node = std::make_shared<SceneNode>(name);
        
        // Basic properties
        node->setVisible(nodeJson.value("visible", true));
        node->setActive(nodeJson.value("active", true));
        
        // Transform
        if (nodeJson.contains("transform")) {
            auto& transform = node->getTransform();
            auto& transformJson = nodeJson["transform"];
            
            if (transformJson.contains("position")) {
                auto pos = transformJson["position"];
                if (pos.is_array() && pos.size() >= 3) {
                    transform.setPosition(glm::vec3(pos[0], pos[1], pos[2]));
                }
            }
            
            if (transformJson.contains("rotation")) {
                auto rot = transformJson["rotation"];
                if (rot.is_array() && rot.size() >= 3) {
                    transform.setEulerAngles(glm::vec3(rot[0], rot[1], rot[2]));
                }
            }
            
            if (transformJson.contains("scale")) {
                auto scale = transformJson["scale"];
                if (scale.is_array() && scale.size() >= 3) {
                    transform.setScale(glm::vec3(scale[0], scale[1], scale[2]));
                }
            }
        }
        
        // Components
        if (nodeJson.contains("components") && nodeJson["components"].is_array()) {
            auto& componentsArray = nodeJson["components"];
#ifdef LINUX_BUILD
            std::cout << "Deserializing node '" << name << "' with " << componentsArray.size() << " component(s)" << std::endl;
#endif
            for (const auto& componentJson : componentsArray) {
                std::string type = componentJson.value("type", "");
#ifdef LINUX_BUILD
                std::cout << "  Processing component type: " << type << std::endl;
#endif
                
                if (type == "CameraComponent") {
                    auto cameraComp = node->addComponent<CameraComponent>();
                    if (componentJson.contains("fov")) {
                        cameraComp->setFOV(componentJson["fov"]);
                    }
                    if (componentJson.contains("nearPlane")) {
                        cameraComp->setNearPlane(componentJson["nearPlane"]);
                    }
                    if (componentJson.contains("farPlane")) {
                        cameraComp->setFarPlane(componentJson["farPlane"]);
                    }
                    // Don't restore active state from JSON here - let scene->setActiveCamera() handle it
                    // This ensures the scene's activeCamera reference and component state stay in sync
                } else if (type == "MeshRenderer") {
                    auto meshRenderer = node->addComponent<MeshRenderer>();
                    
                    if (componentJson.contains("meshType")) {
                        std::string meshType = componentJson["meshType"];
                        std::shared_ptr<Mesh> mesh;
                        
                        if (meshType == "QUAD") {
                            mesh = Mesh::createQuad();
                        } else if (meshType == "PLANE") {
                            mesh = Mesh::createPlane(1.0f, 1.0f, 1);
                        } else if (meshType == "CUBE") {
                            mesh = Mesh::createCube();
                        } else if (meshType == "SPHERE") {
                            mesh = Mesh::createSphere(32, 16);
                        } else if (meshType == "CAPSULE") {
                            mesh = Mesh::createCapsule(0.5f, 0.5f);
                        } else if (meshType == "CYLINDER") {
                            mesh = Mesh::createCylinder(0.5f, 1.0f);
                        } else {
                            mesh = Mesh::createCube(); // Default
                        }
                        
                        meshRenderer->setMesh(mesh);
                    }
                    
                    if (componentJson.contains("material")) {
                        auto material = std::make_shared<Material>();
                        auto& materialJson = componentJson["material"];
                        if (materialJson.contains("color")) {
                            auto color = materialJson["color"];
                            if (color.is_array() && color.size() >= 3) {
                                material->setColor(glm::vec3(color[0], color[1], color[2]));
                            }
                        }
                        
                        // Load texture paths and assign textures
                        auto& textureManager = TextureManager::getInstance();
                        
                        if (materialJson.contains("diffuseTexture")) {
                            std::string diffusePath = materialJson["diffuseTexture"];
                            auto diffuseTexture = textureManager.getTexture(diffusePath);
                            if (diffuseTexture) {
                                material->setDiffuseTexture(diffuseTexture, diffusePath);
                            }
                        }
                        
                        if (materialJson.contains("normalTexture")) {
                            std::string normalPath = materialJson["normalTexture"];
                            auto normalTexture = textureManager.getTexture(normalPath);
                            if (normalTexture) {
                                material->setNormalTexture(normalTexture, normalPath);
                            }
                        }
                        
                        if (materialJson.contains("armTexture")) {
                            std::string armPath = materialJson["armTexture"];
                            auto armTexture = textureManager.getTexture(armPath);
                            if (armTexture) {
                                material->setARMTexture(armTexture, armPath);
                            }
                        }
                        
                        meshRenderer->setMaterial(material);
                    }
                } else if (type == "ModelRenderer") {
                    auto modelRenderer = node->addComponent<ModelRenderer>();
                    
                    if (componentJson.contains("modelPath")) {
                        std::string modelPath = componentJson["modelPath"];
                        modelRenderer->loadModel(modelPath);
                    }
                    
                    if (componentJson.contains("castShadows")) {
                        modelRenderer->setCastShadows(componentJson["castShadows"]);
                    }
                    
                    if (componentJson.contains("receiveShadows")) {
                        modelRenderer->setReceiveShadows(componentJson["receiveShadows"]);
                    }
                } else if (type == "LightComponent") {
                    auto lightComp = node->addComponent<LightComponent>();
                    
                    if (componentJson.contains("lightType")) {
                        std::string lightType = componentJson["lightType"];
                        if (lightType == "POINT") {
                            lightComp->setType(LightType::POINT);
                        } else if (lightType == "DIRECTIONAL") {
                            lightComp->setType(LightType::DIRECTIONAL);
                        } else if (lightType == "SPOT") {
                            lightComp->setType(LightType::SPOT);
                        }
                    }
                    
                    if (componentJson.contains("color")) {
                        auto color = componentJson["color"];
                        if (color.is_array() && color.size() >= 3) {
                            lightComp->setColor(glm::vec3(color[0], color[1], color[2]));
                        }
                    }
                    
                    if (componentJson.contains("intensity")) {
                        lightComp->setIntensity(componentJson["intensity"]);
                    }
                    
                    if (componentJson.contains("range")) {
                        lightComp->setRange(componentJson["range"]);
                    }
                    
                    if (componentJson.contains("showGizmo")) {
                        lightComp->setShowGizmo(componentJson["showGizmo"]);
                    }
                    
                    if (componentJson.contains("direction")) {
                        auto direction = componentJson["direction"];
                        if (direction.is_array() && direction.size() >= 3) {
                            lightComp->setDirection(glm::vec3(direction[0], direction[1], direction[2]));
                        }
                    }
                    
                    if (componentJson.contains("cutOff")) {
                        lightComp->setCutOff(componentJson["cutOff"]);
                    }
                    
                    if (componentJson.contains("outerCutOff")) {
                        lightComp->setOuterCutOff(componentJson["outerCutOff"]);
                    }
                    
                    lightComp->start(); // Initialize the light
                } else if (type == "PhysicsComponent") {
                    auto physicsComp = node->addComponent<PhysicsComponent>();
                    
                    if (componentJson.contains("collisionShapeType")) {
                        std::string shapeType = componentJson["collisionShapeType"];
                        if (shapeType == "BOX") {
                            physicsComp->setCollisionShape(CollisionShapeType::BOX);
                        } else if (shapeType == "SPHERE") {
                            physicsComp->setCollisionShape(CollisionShapeType::SPHERE);
                        } else if (shapeType == "CAPSULE") {
                            physicsComp->setCollisionShape(CollisionShapeType::CAPSULE);
                        } else if (shapeType == "CYLINDER") {
                            physicsComp->setCollisionShape(CollisionShapeType::CYLINDER);
                        } else if (shapeType == "PLANE") {
                            physicsComp->setCollisionShape(CollisionShapeType::PLANE);
                        }
                    }
                    
                    if (componentJson.contains("bodyType")) {
                        std::string bodyType = componentJson["bodyType"];
                        if (bodyType == "STATIC") {
                            physicsComp->setBodyType(PhysicsBodyType::STATIC);
                        } else if (bodyType == "DYNAMIC") {
                            physicsComp->setBodyType(PhysicsBodyType::DYNAMIC);
                        } else if (bodyType == "KINEMATIC") {
                            physicsComp->setBodyType(PhysicsBodyType::KINEMATIC);
                        }
                    }
                    
                    if (componentJson.contains("mass")) {
                        physicsComp->setMass(componentJson["mass"]);
                    }
                    
                    if (componentJson.contains("friction")) {
                        physicsComp->setFriction(componentJson["friction"]);
                    }
                    
                    if (componentJson.contains("restitution")) {
                        physicsComp->setRestitution(componentJson["restitution"]);
                    }
                    
                    if (componentJson.contains("linearDamping")) {
                        physicsComp->setLinearDamping(componentJson["linearDamping"]);
                    }
                    
                    if (componentJson.contains("angularDamping")) {
                        physicsComp->setAngularDamping(componentJson["angularDamping"]);
                    }
                    
                    if (componentJson.contains("showCollisionShape")) {
                        physicsComp->setShowCollisionShape(componentJson["showCollisionShape"]);
                    }
                    
                } else if (type == "TextComponent") {
                    auto textComp = node->addComponent<TextComponent>();
                    
                    if (componentJson.contains("text")) {
                        textComp->setText(componentJson["text"]);
                    }
                    
                    if (componentJson.contains("fontPath")) {
                        textComp->setFontPath(componentJson["fontPath"]);
                    }
                    
                    if (componentJson.contains("fontSize")) {
                        textComp->setFontSize(componentJson["fontSize"]);
                    }
                    
                    if (componentJson.contains("color") && componentJson["color"].is_array() && componentJson["color"].size() >= 4) {
                        glm::vec4 color(
                            componentJson["color"][0],
                            componentJson["color"][1],
                            componentJson["color"][2],
                            componentJson["color"][3]
                        );
                        textComp->setColor(color);
                    }
                    
                    if (componentJson.contains("renderMode")) {
                        std::string renderMode = componentJson["renderMode"];
                        if (renderMode == "WORLD_SPACE") {
                            textComp->setRenderMode(TextRenderMode::WORLD_SPACE);
                        } else if (renderMode == "SCREEN_SPACE") {
                            textComp->setRenderMode(TextRenderMode::SCREEN_SPACE);
                        }
                    }
                    
                    if (componentJson.contains("alignment")) {
                        std::string alignment = componentJson["alignment"];
                        if (alignment == "LEFT") {
                            textComp->setAlignment(TextAlignment::LEFT);
                        } else if (alignment == "CENTER") {
                            textComp->setAlignment(TextAlignment::CENTER);
                        } else if (alignment == "RIGHT") {
                            textComp->setAlignment(TextAlignment::RIGHT);
                        }
                    }
                    
                    if (componentJson.contains("scale")) {
                        textComp->setScale(componentJson["scale"]);
                    }
                    
                    if (componentJson.contains("lineSpacing")) {
                        textComp->setLineSpacing(componentJson["lineSpacing"]);
                    }
                    
                    textComp->start(); // Initialize the text component
                } else if (type == "ScriptComponent") {
                    auto scriptComp = node->addComponent<ScriptComponent>();
                    
                    // Check for both "scriptPath" (current) and "script" (legacy) for compatibility
                    std::string scriptPath = "";
                    if (componentJson.contains("scriptPath") && !componentJson["scriptPath"].is_null()) {
                        scriptPath = componentJson["scriptPath"];
                    } else if (componentJson.contains("script") && !componentJson["script"].is_null()) {
                        scriptPath = componentJson["script"]; // Legacy support
                    }
                    
                    if (!scriptPath.empty()) {
                        scriptComp->loadScript(scriptPath);
                        // Don't start scripts during deserialization, they will be started
                        // when Scene::start() is called after the entire scene is loaded
                        // This ensures all nodes and components are available when scripts start
                    } else {
#ifdef LINUX_BUILD
                        std::cout << "ScriptComponent on node \"" << name << "\" has empty script path, skipping start()" << std::endl;
#endif
                    }
                    
                    // Load pause exempt setting
                    if (componentJson.contains("pauseExempt") && componentJson["pauseExempt"].is_boolean()) {
                        bool pauseExempt = componentJson["pauseExempt"];
                        scriptComp->setPauseExempt(pauseExempt);
#ifdef LINUX_BUILD
                        std::cout << "ScriptComponent on node \"" << name << "\" pauseExempt set to: " << (pauseExempt ? "true" : "false") << std::endl;
#endif
                    } else {
#ifdef LINUX_BUILD
                        std::cout << "ScriptComponent on node \"" << name << "\" pauseExempt not found in JSON, defaulting to false" << std::endl;
#endif
                    }
                } else if (type == "Area3DComponent") {
#ifdef LINUX_BUILD
                    std::cout << "Deserializing Area3DComponent for node: " << name << std::endl;
#endif
                    auto area3DComp = node->addComponent<Area3DComponent>();
                    if (!area3DComp) {
#ifdef LINUX_BUILD
                        std::cerr << "ERROR: Failed to add Area3DComponent to node: " << name << std::endl;
#endif
                    } else {
#ifdef LINUX_BUILD
                        std::cout << "Successfully added Area3DComponent to node: " << name << std::endl;
#endif
                    }
                    
                    if (componentJson.contains("shapeType")) {
                        std::string shapeTypeStr = componentJson["shapeType"];
                        if (shapeTypeStr == "BOX") {
                            area3DComp->setShape(Area3DShape::BOX);
                        } else if (shapeTypeStr == "SPHERE") {
                            area3DComp->setShape(Area3DShape::SPHERE);
                        } else if (shapeTypeStr == "CAPSULE") {
                            area3DComp->setShape(Area3DShape::CAPSULE);
                        } else if (shapeTypeStr == "CYLINDER") {
                            area3DComp->setShape(Area3DShape::CYLINDER);
                        } else if (shapeTypeStr == "PLANE") {
                            area3DComp->setShape(Area3DShape::PLANE);
                        }
                    }
                    
                    if (componentJson.contains("dimensions")) {
                        auto dims = componentJson["dimensions"];
                        if (dims.is_array() && dims.size() >= 3) {
                            area3DComp->setDimensions(glm::vec3(dims[0], dims[1], dims[2]));
                        }
                    }
                    
                    if (componentJson.contains("radius")) {
                        area3DComp->setRadius(componentJson["radius"]);
                    }
                    
                    if (componentJson.contains("height")) {
                        area3DComp->setHeight(componentJson["height"]);
                    }
                    
                    if (componentJson.contains("group")) {
                        area3DComp->setGroup(componentJson["group"]);
                    }
                    
                    if (componentJson.contains("monitorMode")) {
                        area3DComp->setMonitorMode(componentJson["monitorMode"]);
                    }
                    
                    if (componentJson.contains("showDebugShape")) {
                        area3DComp->setShowDebugShape(componentJson["showDebugShape"]);
                    }
                    
                    area3DComp->start(); // Initialize the area3D component
                } else if (type == "AnimationComponent") {
                    auto animComp = node->addComponent<AnimationComponent>();
                    
                    if (componentJson.contains("skeletonName")) {
                        std::string skeletonName = componentJson["skeletonName"];
                        animComp->setSkeleton(skeletonName);
                    }
                    
                    if (componentJson.contains("animationClipName")) {
                        std::string clipName = componentJson["animationClipName"];
                        animComp->setAnimationClip(clipName);
                    }
                    
                    if (componentJson.contains("loop")) {
                        animComp->setLoop(componentJson["loop"]);
                    }
                    
                    if (componentJson.contains("speed")) {
                        animComp->setSpeed(componentJson["speed"]);
                    }
                    
                    if (componentJson.contains("enableRootMotion")) {
                        animComp->setRootMotionEnabled(componentJson["enableRootMotion"]);
                    }
                    
                    if (componentJson.contains("autoPlay") && componentJson["autoPlay"]) {
                        animComp->play();
                    }
                }
            }
        }
        
        // Children
        if (nodeJson.contains("children") && nodeJson["children"].is_array()) {
            auto& childrenArray = nodeJson["children"];
            for (const auto& childJson : childrenArray) {
                auto child = deserializeNodeFromJson(childJson);
                if (child) {
                    node->addChild(child);
                }
            }
        }
        
        return node;
#ifndef VITA_BUILD
    } catch (const json::exception& e) {
        std::cerr << "Error parsing node JSON: " << e.what() << std::endl;
        return nullptr;
    } catch (const std::exception& e) {
        std::cerr << "Error deserializing node from JSON: " << e.what() << std::endl;
        return nullptr;
    }
#endif
}

#ifdef LINUX_BUILD
std::vector<std::string> SceneSerializer::discoverAndGenerateTextureAssets() {
    std::vector<std::string> discoveredTextures;
    
    try {
        // Use TextureManager to discover all textures recursively
        auto& textureManager = TextureManager::getInstance();
        discoveredTextures = textureManager.discoverAllTextures("assets/textures");
        
        std::cout << "Discovered " << discoveredTextures.size() << " textures for build system" << std::endl;
        
        // Generate texture manifest for Vita builds
        generateTextureManifest(discoveredTextures);
        
        return discoveredTextures;
    } catch (const std::exception& ex) {
        std::cerr << "Error discovering textures: " << ex.what() << std::endl;
        return discoveredTextures;
    }
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
void SceneSerializer::generateInputMappingAssets() {
    try {
        // Check if input_mappings.txt exists
        std::ifstream inputFile("input_mappings.txt");
        if (!inputFile.is_open()) {
            std::cout << "No input_mappings.txt found, creating default one..." << std::endl;
            
            // Create default input mappings file
            std::ofstream defaultFile("input_mappings.txt");
            if (defaultFile.is_open()) {
                defaultFile << R"(action:MoveForward,0,87,1,0.1,1.0
action:MoveBackward,0,83,1,0.1,1.0
action:MoveLeft,0,65,1,0.1,1.0
action:MoveRight,0,68,1,0.1,1.0
action:MoveUp,0,32,1,0.1,1.0
action:MoveDown,0,340,1,0.1,1.0
action:Jump,0,32,0,0.1,1.0
action:Run,0,341,1,0.1,1.0
action:Interact,0,69,0,0.1,1.0
action:Menu,0,256,0,0.1,1.0
action:LookHorizontal,3,0,1,0.1,1.0
action:LookVertical,3,1,1,0.1,1.0
action:PrimaryFire,2,0,1,0.1,1.0
action:SecondaryFire,2,1,1,0.1,1.0
action:MoveHorizontal,2,0,1,0.1,1.0
action:MoveVertical,2,0,1,0.1,1.0
action:LookHorizontal,2,1,1,0.1,1.0
action:LookVertical,2,1,1,0.1,1.0
)";
                defaultFile.close();
                std::cout << "Created default input_mappings.txt" << std::endl;
            }
        } else {
            inputFile.close();
            std::cout << "Found existing input_mappings.txt" << std::endl;
        }
        
        // Generate input mapping manifest for Vita builds
        generateInputMappingManifest();
        
    } catch (const std::exception& ex) {
        std::cerr << "Error generating input mapping assets: " << ex.what() << std::endl;
    }
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
void SceneSerializer::generateInputMappingManifest() {
    // First, read the existing input mappings file into memory
    std::vector<std::string> existingMappings;
    std::ifstream inputFile("input_mappings.txt");
    if (inputFile.is_open()) {
        std::string line;
        while (std::getline(inputFile, line)) {
            if (line.find("action:") != std::string::npos) {
                existingMappings.push_back(line);
            }
        }
        inputFile.close();
    }
    
    // Now write the file with header and existing mappings
    std::ofstream manifestFile("input_mappings.txt");
    if (manifestFile.is_open()) {
        manifestFile << "# Input Mappings Configuration\n";
        manifestFile << "# Format: action:name,type,code,actionType,deadzone,sensitivity\n";
        manifestFile << "# Types: 0=Keyboard, 1=Vita Button, 2=Analog Stick, 3=Mouse Button, 4=Mouse Axis\n";
        manifestFile << "# Action Types: 0=Pressed, 1=Held, 2=Released\n\n";
        
        // Write all the existing mappings
        for (const auto& line : existingMappings) {
            manifestFile << line << "\n";
        }
        
        manifestFile.close();
        std::cout << "Generated input mapping manifest" << std::endl;
    }
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
std::vector<std::string> SceneSerializer::discoverAndGenerateFontAssets() {
    std::vector<std::string> discoveredFonts;
    
    try {
        // Discover all font files in assets/fonts directory
        std::string fontsDir = "assets/fonts";
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fontsDir)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                // Convert backslashes to forward slashes for consistency
                std::replace(filePath.begin(), filePath.end(), '\\', '/');
                
                // Check if it's a font file
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".ttf" || extension == ".otf" || extension == ".woff" || extension == ".woff2") {
                    discoveredFonts.push_back(filePath);
                }
            }
        }
        
        std::cout << "Discovered " << discoveredFonts.size() << " fonts for build system" << std::endl;
        
        // Generate font manifest for Vita builds
        generateFontManifest(discoveredFonts);
        
        return discoveredFonts;
    } catch (const std::exception& e) {
        std::cerr << "Error discovering fonts: " << e.what() << std::endl;
        return discoveredFonts;
    }
}

std::vector<std::string> SceneSerializer::discoverAndGenerateModelAssets() {
    std::vector<std::string> discoveredModels;
    
    try {
        // Discover all GLTF and GLB files in the models directory
        std::string modelsDir = "assets/models";
        
        // Use filesystem to recursively find model files
        for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsDir)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string extension = entry.path().extension().string();
                
                // Convert extension to lowercase for comparison
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".gltf" || extension == ".glb" || extension == ".bin" || 
                    extension == ".jpg" || extension == ".jpeg" || extension == ".png" || 
                    extension == ".bmp" || extension == ".tga") {
                    discoveredModels.push_back(filePath);
                }
            }
        }
        
        std::cout << "Discovered " << discoveredModels.size() << " model files for build system" << std::endl;
        
        // Generate model manifest for Vita builds
        generateModelManifest(discoveredModels);
        
        return discoveredModels;
    } catch (const std::exception& ex) {
        std::cerr << "Error discovering models: " << ex.what() << std::endl;
        return discoveredModels;
    }
}

std::vector<std::string> SceneSerializer::discoverAndGenerateScriptAssets() {
    std::vector<std::string> discoveredScripts;
    
    try {
        // Discover all Lua script files in the scripts directory
        std::string scriptsDir = "scripts";
        
        // Use filesystem to recursively find script files
        for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsDir)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string extension = entry.path().extension().string();
                
                // Convert extension to lowercase for comparison
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                // Check for Lua script files
                if (extension == ".lua") {
                    // Convert backslashes to forward slashes for consistency
                    std::replace(filePath.begin(), filePath.end(), '\\', '/');
                    discoveredScripts.push_back(filePath);
                }
            }
        }
        
        std::cout << "Discovered " << discoveredScripts.size() << " scripts for build system" << std::endl;
        
        // Generate script manifest for Vita builds
        generateScriptManifest(discoveredScripts);
        
        return discoveredScripts;
    } catch (const std::exception& ex) {
        std::cerr << "Error discovering scripts: " << ex.what() << std::endl;
        return discoveredScripts;
    }
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
void SceneSerializer::updateMakefileWithTextures(const std::vector<std::string>& discoveredTextures) {
    // Also discover fonts, models, and scripts and include them
    auto discoveredFonts = discoverAndGenerateFontAssets();
    auto discoveredModels = discoverAndGenerateModelAssets();
    auto discoveredScripts = discoverAndGenerateScriptAssets();
    updateMakefileWithAssets(discoveredTextures, discoveredFonts, discoveredModels, discoveredScripts);
}

void SceneSerializer::updateMakefileWithAssets(const std::vector<std::string>& discoveredTextures, const std::vector<std::string>& discoveredFonts, const std::vector<std::string>& discoveredModels, const std::vector<std::string>& discoveredScripts) {
    try {
        // Read current Makefile
        std::ifstream makefileIn("Makefile");
        if (!makefileIn.is_open()) {
            std::cerr << "Failed to open Makefile for reading" << std::endl;
            return;
        }
        
        std::string makefileContent((std::istreambuf_iterator<char>(makefileIn)),
                                   std::istreambuf_iterator<char>());
        makefileIn.close();
        
        // Find the VPK creation section
        std::string vpkStartMarker = "$(BUILD_DIR)/$(TARGET).vpk: $(BUILD_DIR)/eboot.bin";
        std::string vpkEndMarker = "$@";
        
        size_t vpkStart = makefileContent.find(vpkStartMarker);
        if (vpkStart == std::string::npos) {
            std::cerr << "Could not find VPK creation section in Makefile" << std::endl;
            return;
        }
        
        // Find the end of the VPK command
        size_t vpkEnd = makefileContent.find(vpkEndMarker, vpkStart);
        if (vpkEnd == std::string::npos) {
            std::cerr << "Could not find end of VPK command in Makefile" << std::endl;
            return;
        }
        
        // Extract the part before the VPK command
        std::string beforeVpk = makefileContent.substr(0, vpkStart);
        
        // Extract the part after the VPK command
        std::string afterVpk = makefileContent.substr(vpkEnd + vpkEndMarker.length());
        
        // Build new VPK command with texture assets
        std::string newVpkCommand = vpkStartMarker + "\n";
        newVpkCommand += "\tvita-mksfoex -s TITLE_ID=$(TITLEID) \"$(TARGET)\" $(BUILD_DIR)/param.sfo\n";
        newVpkCommand += "\tvita-pack-vpk -s $(BUILD_DIR)/param.sfo -b $(BUILD_DIR)/eboot.bin \\\n";
        
        // Add existing assets
        newVpkCommand += "\t\t-a assets/shaders/lambertian.vert=lambertian.vert \\\n";
        newVpkCommand += "\t\t-a assets/shaders/lambertian.frag=lambertian.frag \\\n";
        newVpkCommand += "\t\t-a assets/shaders/lambertian_hlsl.vert=lambertian_hlsl.vert \\\n";
        newVpkCommand += "\t\t-a assets/shaders/lambertian_hlsl.frag=lambertian_hlsl.frag \\\n";
        newVpkCommand += "\t\t-a assets/shaders/lighting.vert=lighting.vert \\\n";
        newVpkCommand += "\t\t-a assets/shaders/lighting.frag=lighting.frag \\\n";
        newVpkCommand += "\t\t-a assets/shaders/text.vert=text.vert \\\n";
        newVpkCommand += "\t\t-a assets/shaders/text.frag=text.frag \\\n";
        newVpkCommand += "\t\t-a textures.txt=textures.txt \\\n";
        newVpkCommand += "\t\t-a fonts.txt=fonts.txt \\\n";
        newVpkCommand += "\t\t-a scripts.txt=scripts.txt \\\n";
        newVpkCommand += "\t\t-a input_mappings.txt=input_mappings.txt \\\n";
        
        // Add texture assets
        for (const auto& texturePath : discoveredTextures) {
            // Extract just the filename from the full path
            std::string filename = texturePath;
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            newVpkCommand += "\t\t-a " + texturePath + "=" + filename + " \\\n";
        }
        
        // Add font assets
        for (const auto& fontPath : discoveredFonts) {
            // Extract just the filename from the full path
            std::string filename = fontPath;
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            newVpkCommand += "\t\t-a " + fontPath + "=" + fontPath + " \\\n";
        }
        
        // Add model assets
        for (const auto& modelPath : discoveredModels) {
            // For GLTF files, preserve the directory structure in VPK
            // e.g., assets/models/lemon_1k.gltf/lemon_1k.gltf -> models/lemon_1k.gltf/lemon_1k.gltf
            std::string vpkPath = modelPath;
            // Remove the "assets/" prefix for VPK mapping
            if (vpkPath.find("assets/") == 0) {
                vpkPath = vpkPath.substr(7); // Remove "assets/" (7 characters)
            }
            newVpkCommand += "\t\t-a " + modelPath + "=" + vpkPath + " \\\n";
        }
        
        // Add script assets
        for (const auto& scriptPath : discoveredScripts) {
            // Preserve the directory structure for scripts in VPK
            // e.g., scripts/player_behavior.lua -> scripts/player_behavior.lua
            newVpkCommand += "\t\t-a " + scriptPath + "=" + scriptPath + " \\\n";
        }
        
        // Remove the last backslash and add the target
        if (!newVpkCommand.empty() && newVpkCommand.back() == '\\') {
            newVpkCommand.pop_back();
            newVpkCommand.pop_back(); // Remove the space too
        }
        newVpkCommand += " $@\n";
        
        // Trim leading whitespace/newlines from afterVpk to avoid extra blank lines
        size_t afterVpkStart = 0;
        while (afterVpkStart < afterVpk.length() && (afterVpk[afterVpkStart] == ' ' || afterVpk[afterVpkStart] == '\t' || afterVpk[afterVpkStart] == '\n' || afterVpk[afterVpkStart] == '\r')) {
            afterVpkStart++;
        }
        std::string trimmedAfterVpk = afterVpk.substr(afterVpkStart);
        
        // Reconstruct the Makefile
        std::string newMakefileContent = beforeVpk + newVpkCommand;
        // Only add trimmed afterVpk if it's not empty
        if (!trimmedAfterVpk.empty()) {
            newMakefileContent += trimmedAfterVpk;
        }
        
        // Write the updated Makefile
        std::ofstream makefileOut("Makefile");
        if (makefileOut.is_open()) {
            makefileOut << newMakefileContent;
            makefileOut.close();
            std::cout << "Makefile updated with " << discoveredTextures.size() << " texture assets, "
                      << discoveredFonts.size() << " font assets, " << discoveredModels.size() << " model assets, and "
                      << discoveredScripts.size() << " script assets" << std::endl;
        } else {
            std::cerr << "Failed to open Makefile for writing" << std::endl;
        }
        
    } catch (const std::exception& ex) {
        std::cerr << "Error updating Makefile: " << ex.what() << std::endl;
    }
}
#endif // LINUX_BUILD

std::string SceneSerializer::convertToVitaPath(const std::string& path) {
    // Convert absolute Linux path to relative Vita path
    // e.g., /home/user/project/assets/models/lemon_1k.gltf/lemon_1k.gltf -> models/lemon_1k.gltf/lemon_1k.gltf
    
    std::string vitaPath = path;
    
    // Remove the "assets/" prefix if present
    if (vitaPath.find("assets/") != std::string::npos) {
        size_t assetsPos = vitaPath.find("assets/");
        vitaPath = vitaPath.substr(assetsPos + 7); // Remove "assets/" (7 characters)
    }
    
    return vitaPath;
}

std::string SceneSerializer::convertToLinuxPath(const std::string& path) {
    // For Linux builds, keep the full path as-is
    return path;
}

#ifdef LINUX_BUILD
void SceneSerializer::generateTextureManifest(const std::vector<std::string>& discoveredTextures) {
    try {
        // Write texture manifest to textures.txt
        std::ofstream manifestFile("textures.txt");
        if (!manifestFile.is_open()) {
            std::cerr << "Failed to create texture manifest file" << std::endl;
            return;
        }
        
        for (const auto& texturePath : discoveredTextures) {
            // Extract just the filename from the full path
            std::string filename = texturePath;
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            manifestFile << filename << std::endl;
        }
        
        manifestFile.close();
        std::cout << "Generated texture manifest with " << discoveredTextures.size() << " textures" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Error generating texture manifest: " << ex.what() << std::endl;
    }
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
void SceneSerializer::generateFontManifest(const std::vector<std::string>& discoveredFonts) {
    try {
        // Write font manifest to fonts.txt
        std::ofstream manifestFile("fonts.txt");
        if (!manifestFile.is_open()) {
            std::cerr << "Failed to create font manifest file" << std::endl;
            return;
        }
        
        for (const auto& fontPath : discoveredFonts) {
            // Extract just the filename from the full path
            std::string filename = fontPath;
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            manifestFile << filename << std::endl;
        }
        
        manifestFile.close();
        std::cout << "Generated font manifest with " << discoveredFonts.size() << " fonts" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Error generating font manifest: " << ex.what() << std::endl;
    }
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
void SceneSerializer::generateScriptManifest(const std::vector<std::string>& discoveredScripts) {
    try {
        // Write script manifest to scripts.txt
        std::ofstream manifestFile("scripts.txt");
        if (!manifestFile.is_open()) {
            std::cerr << "Failed to create script manifest file" << std::endl;
            return;
        }
        
        for (const auto& scriptPath : discoveredScripts) {
            // Extract just the filename from the full path
            std::string filename = scriptPath;
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            manifestFile << filename << std::endl;
        }
        
        manifestFile.close();
        std::cout << "Generated script manifest with " << discoveredScripts.size() << " scripts" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Error generating script manifest: " << ex.what() << std::endl;
    }
}
#endif // LINUX_BUILD

#ifdef LINUX_BUILD
void SceneSerializer::generateModelManifest(const std::vector<std::string>& discoveredModels) {
    try {
        // Write model manifest to models.txt
        std::ofstream manifestFile("models.txt");
        if (!manifestFile.is_open()) {
            std::cerr << "Failed to create model manifest file" << std::endl;
            return;
        }
        
        for (const auto& modelPath : discoveredModels) {
            // Convert to Vita path (remove assets/ prefix but keep directory structure)
            std::string vitaPath = convertToVitaPath(modelPath);
            manifestFile << vitaPath << std::endl;
        }
        
        manifestFile.close();
        std::cout << "Generated model manifest with " << discoveredModels.size() << " models" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Error generating model manifest: " << ex.what() << std::endl;
    }
}
#endif // LINUX_BUILD

} // namespace GameEngine
