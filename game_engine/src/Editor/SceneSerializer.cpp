#include "Editor/SceneSerializer.h"
#include "Core/AssetPaths.h"
#include "Scene/SceneBinaryFormat.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/CameraComponent.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/MaterialComponent.h"
#include "Components/BeamRenderer.h"
#include "Components/LightComponent.h"
#include "Components/PhysicsComponent.h"
#include "Components/TextComponent.h"
#include "Physics/PhysicsManager.h"
#include "Components/ScriptComponent.h"
#include "Components/Area3DComponent.h"
#include "Components/RaycastComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/SoundComponent.h"
#include "Components/SkyboxComponent.h"
#include "Components/JointComponent.h"
#include "Components/NavObstacleComponent.h"
#include "Components/NavAgentComponent.h"
#include "Components/NavVolumeComponent.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/ShaderManager.h"
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

#ifdef LINUX_BUILD
void SceneSerializer::generateAssetManifests() {
    // Refreshes the manifests and Makefile asset list the Vita packing reads.
    //
    // This used to also write game_main.cpp and vita_main.cpp from the scene.
    // Generating tracked source from the editor means a click can overwrite hand
    // written startup code and break the build, so the scene the game boots into
    // is a project setting now (project:mainScene) and the entry points read it.
    const std::vector<std::string> discoveredTextures = discoverAndGenerateTextureAssets();
    generateInputMappingAssets();
    updateMakefileWithTextures(discoveredTextures);

    std::cout << "Asset manifests updated." << std::endl;
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
    const std::string loadPath = SceneBinaryFormat::resolveSceneLoadPath(filepath);
    std::vector<uint8_t> fileBytes;
    if (!SceneBinaryFormat::readFileBytes(loadPath, fileBytes) || fileBytes.empty()) {
#ifdef VITA_BUILD
        printf("Failed to read scene file: %s (resolved: %s)\n", filepath.c_str(), loadPath.c_str());
#else
        std::cerr << "Failed to read scene file: " << filepath << " (resolved: " << loadPath << ")" << std::endl;
#endif
        return nullptr;
    }

    std::shared_ptr<Scene> scene = loadSceneFromBytes(fileBytes, loadPath);
    if (scene) {
#ifdef VITA_BUILD
        printf("Scene loaded successfully from: %s\n", loadPath.c_str());
#else
        std::cout << "Scene loaded successfully from: " << loadPath << std::endl;
#endif
    }

    return scene;
}

std::shared_ptr<Scene> SceneSerializer::loadSceneFromBytes(const std::vector<uint8_t>& fileBytes, const std::string& sourceLabel) {
    if (SceneBinaryFormat::isBinaryPayload(fileBytes)) {
        json sceneJson = json::from_msgpack(
            fileBytes.data() + 8,
            fileBytes.data() + fileBytes.size()
#ifdef VITA_BUILD
            , false, false
#endif
        );

#ifdef VITA_BUILD
        if (sceneJson.is_discarded()) {
            printf("SceneSerializer: Failed to decode binary scene: %s\n", sourceLabel.c_str());
            return nullptr;
        }
#else
        (void)sourceLabel;
#endif

        return deserializeSceneFromJson(sceneJson);
    }

    const std::string jsonText(fileBytes.begin(), fileBytes.end());
    return deserializeSceneFromJsonText(jsonText);
}

std::shared_ptr<Scene> SceneSerializer::deserializeSceneFromJsonText(const std::string& jsonData) {
#ifdef VITA_BUILD
    json sceneJson = json::parse(jsonData.begin(), jsonData.end(), nullptr, false);
    if (sceneJson.is_discarded()) {
        return nullptr;
    }
    return deserializeSceneFromJson(sceneJson);
#else
    try {
        json sceneJson = json::parse(jsonData);
        return deserializeSceneFromJson(sceneJson);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return nullptr;
    }
#endif
}

std::string SceneSerializer::serializeSceneToJson(std::shared_ptr<Scene> scene) {
    if (!scene) return "{}";
    
    json sceneJson;
    sceneJson["name"] = scene->getName();
    sceneJson["version"] = "1.0";
    
    // Serialize physics world gravity
    glm::vec3 gravity = PhysicsManager::getInstance().getGravity();
    sceneJson["physics"]["gravity"] = { gravity.x, gravity.y, gravity.z };
    
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
    
    auto activeSkybox = scene->getActiveSkybox();
    if (activeSkybox) {
        sceneJson["activeSkybox"] = activeSkybox->getName();
    } else {
        sceneJson["activeSkybox"] = nullptr;
    }
    
    return sceneJson.dump(2);
}

std::shared_ptr<Scene> SceneSerializer::deserializeSceneFromJson(const json& sceneJson) {
#ifndef VITA_BUILD
    try {
#endif
        std::string sceneName = sceneJson.value("name", "Loaded Scene");
        auto scene = std::make_shared<Scene>(sceneName);
        
        scene->getRootNode()->removeAllChildren();
        
        if (sceneJson.contains("physics") && sceneJson["physics"].is_object() && sceneJson["physics"].contains("gravity") && sceneJson["physics"]["gravity"].is_array() && sceneJson["physics"]["gravity"].size() >= 3) {
            glm::vec3 gravity(
                sceneJson["physics"]["gravity"][0],
                sceneJson["physics"]["gravity"][1],
                sceneJson["physics"]["gravity"][2]
            );
            PhysicsManager::getInstance().setGravity(gravity);
        }
        
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
        
        if (sceneJson.contains("activeCamera") && !sceneJson["activeCamera"].is_null()) {
            std::string activeCameraName = sceneJson["activeCamera"];
            auto cameraNode = scene->findNode(activeCameraName);
            if (cameraNode) {
                scene->setActiveCamera(cameraNode);
            }
        }
        
        if (sceneJson.contains("activeSkybox") && !sceneJson["activeSkybox"].is_null()) {
            std::string activeSkyboxName = sceneJson["activeSkybox"];
            auto skyboxNode = scene->findNode(activeSkyboxName);
            if (skyboxNode) {
                scene->setActiveSkybox(skyboxNode);
            }
        }
        
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
                                case MeshType::RAMP: meshType = "RAMP"; break;
                                case MeshType::LINE: meshType = "LINE"; break;
                                case MeshType::BEAM: meshType = "BEAM"; break;
                                default: meshType = "CUBE"; break;
                            }
                            componentJson["meshType"] = meshType;
                        }
                        
                        auto material = meshRenderer->getMaterial();
                        if (material) {
                            auto color = material->getColor();
                            json materialJson;
                            materialJson["color"] = {color.x, color.y, color.z};
                            
                            materialJson["metallic"] = material->getMetallic();
                            materialJson["roughness"] = material->getRoughness();
                            materialJson["reflectionStrength"] = material->getReflectionStrength();
                            materialJson["opacity"] = material->getOpacity();
                            materialJson["depthWrite"] = material->getDepthWrite();
                            auto uvScale = material->getUVScale();
                            auto uvOffset = material->getUVOffset();
                            materialJson["uvScale"] = {uvScale.x, uvScale.y};
                            materialJson["uvOffset"] = {uvOffset.x, uvOffset.y};
                            switch (material->getBlendMode()) {
                                case BlendMode::Opaque:  materialJson["blendMode"] = "Opaque"; break;
                                case BlendMode::Alpha:   materialJson["blendMode"] = "Alpha"; break;
                                case BlendMode::Additive: materialJson["blendMode"] = "Additive"; break;
                            }
                            if (material->hasDiffuseTexture()) {
                                std::string diffusePath = material->getDiffuseTexturePath();
                                if (!diffusePath.empty()) {
                                    materialJson["diffuseTexture"] = diffusePath;
                                }
                            }
                            if (material->hasNormalTexture()) {
                                std::string normalPath = material->getNormalTexturePath();
                                if (!normalPath.empty()) {
                                    materialJson["normalTexture"] = normalPath;
                                }
                            }
                            if (material->hasARMTexture()) {
                                std::string armPath = material->getARMTexturePath();
                                if (!armPath.empty()) {
                                    materialJson["armTexture"] = armPath;
                                }
                            }
                            if (material->hasEnvironmentTexture()) {
                                std::string environmentPath = material->getEnvironmentTexturePath();
                                if (!environmentPath.empty()) {
                                    materialJson["environmentTexture"] = environmentPath;
                                }
                            }
                            
                            std::string linuxVertexPath = material->getShaderVertexPathForPlatform("linux");
                            std::string linuxFragmentPath = material->getShaderFragmentPathForPlatform("linux");
                            std::string vitaVertexPath = material->getShaderVertexPathForPlatform("vita");
                            std::string vitaFragmentPath = material->getShaderFragmentPathForPlatform("vita");
                            std::string vulkanVertexPath = material->getShaderVertexPathForPlatform("vulkan");
                            std::string vulkanFragmentPath = material->getShaderFragmentPathForPlatform("vulkan");
                            
                            if (!linuxVertexPath.empty() && !linuxFragmentPath.empty()) {
                                materialJson["shaderVertexPathLinux"] = linuxVertexPath;
                                materialJson["shaderFragmentPathLinux"] = linuxFragmentPath;
                            }
                            
                            if (!vitaVertexPath.empty() && !vitaFragmentPath.empty()) {
                                materialJson["shaderVertexPathVita"] = vitaVertexPath;
                                materialJson["shaderFragmentPathVita"] = vitaFragmentPath;
                            }

                            if (!vulkanVertexPath.empty() && !vulkanFragmentPath.empty()) {
                                materialJson["shaderVertexPathVulkan"] = vulkanVertexPath;
                                materialJson["shaderFragmentPathVulkan"] = vulkanFragmentPath;
                            }
                            
                            if (material->isUsingCustomShader()) {
                                std::string vertexPath = material->getShaderVertexPath();
                                std::string fragmentPath = material->getShaderFragmentPath();
                                if (!vertexPath.empty() && !fragmentPath.empty()) {
                                    materialJson["shaderVertexPath"] = vertexPath;
                                    materialJson["shaderFragmentPath"] = fragmentPath;
                                }
                            }
                            json customTexArray = json::array();
                            for (const auto& entry : material->getCustomTextureUniforms()) {
                                std::string path = entry.second;
                                size_t rel = path.find("assets/textures");
                                if (rel != std::string::npos) path = path.substr(rel);
                                customTexArray.push_back({{"name", entry.first}, {"path", path}});
                            }
                            if (!customTexArray.empty()) materialJson["customTextureUniforms"] = customTexArray;
                            
                            componentJson["material"] = materialJson;
                        }
                    }
                } else if (component->getTypeName() == "BeamRenderer") {
                    auto beamRenderer = node->getComponent<BeamRenderer>();
                    if (beamRenderer) {
                        componentJson["beamWidth"] = beamRenderer->getBeamWidth();
                        auto material = beamRenderer->getMaterial();
                        if (material) {
                            json materialJson;
                            auto color = material->getColor();
                            materialJson["color"] = {color.x, color.y, color.z};
                            materialJson["metallic"] = material->getMetallic();
                            materialJson["roughness"] = material->getRoughness();
                            materialJson["reflectionStrength"] = material->getReflectionStrength();
                            materialJson["opacity"] = material->getOpacity();
                            materialJson["depthWrite"] = material->getDepthWrite();
                            auto uvScale = material->getUVScale();
                            auto uvOffset = material->getUVOffset();
                            materialJson["uvScale"] = {uvScale.x, uvScale.y};
                            materialJson["uvOffset"] = {uvOffset.x, uvOffset.y};
                            switch (material->getBlendMode()) {
                                case BlendMode::Opaque:   materialJson["blendMode"] = "Opaque"; break;
                                case BlendMode::Alpha:    materialJson["blendMode"] = "Alpha"; break;
                                case BlendMode::Additive: materialJson["blendMode"] = "Additive"; break;
                            }
                            std::string linuxVertexPath = material->getShaderVertexPathForPlatform("linux");
                            std::string linuxFragmentPath = material->getShaderFragmentPathForPlatform("linux");
                            std::string vitaVertexPath = material->getShaderVertexPathForPlatform("vita");
                            std::string vitaFragmentPath = material->getShaderFragmentPathForPlatform("vita");
                            std::string vulkanVertexPath = material->getShaderVertexPathForPlatform("vulkan");
                            std::string vulkanFragmentPath = material->getShaderFragmentPathForPlatform("vulkan");
                            if (!linuxVertexPath.empty()) materialJson["shaderVertexPathLinux"] = linuxVertexPath;
                            if (!linuxFragmentPath.empty()) materialJson["shaderFragmentPathLinux"] = linuxFragmentPath;
                            if (!vitaVertexPath.empty()) materialJson["shaderVertexPathVita"] = vitaVertexPath;
                            if (!vitaFragmentPath.empty()) materialJson["shaderFragmentPathVita"] = vitaFragmentPath;
                            if (!vulkanVertexPath.empty()) materialJson["shaderVertexPathVulkan"] = vulkanVertexPath;
                            if (!vulkanFragmentPath.empty()) materialJson["shaderFragmentPathVulkan"] = vulkanFragmentPath;
                            json customTexArray = json::array();
                            for (const auto& entry : material->getCustomTextureUniforms()) {
                                std::string path = entry.second;
                                size_t rel = path.find("assets/textures");
                                if (rel != std::string::npos) path = path.substr(rel);
                                customTexArray.push_back({{"name", entry.first}, {"path", path}});
                            }
                            if (!customTexArray.empty()) materialJson["customTextureUniforms"] = customTexArray;
                            componentJson["material"] = materialJson;
                        }
                    }
                } else if (component->getTypeName() == "ModelRenderer") {
                    auto modelRenderer = node->getComponent<ModelRenderer>();
                    if (modelRenderer) {
                        componentJson["modelPath"] = AssetPaths::toPortable(modelRenderer->getModelPath());
                        componentJson["modelName"] = modelRenderer->getModelName();
                        componentJson["isLoaded"] = modelRenderer->isModelLoaded();
                        componentJson["castShadows"] = modelRenderer->getCastShadows();
                        componentJson["receiveShadows"] = modelRenderer->getReceiveShadows();
                    }
                } else if (component->getTypeName() == "MaterialComponent") {
                    auto materialComponent = node->getComponent<MaterialComponent>();
                    if (materialComponent) {
                        const MaterialOverride& overrides = materialComponent->getOverrides();

                        componentJson["overrideBaseColor"] = overrides.overrideBaseColor;
                        componentJson["baseColor"] = {overrides.baseColor.x, overrides.baseColor.y, overrides.baseColor.z};
                        componentJson["overrideMetallic"] = overrides.overrideMetallic;
                        componentJson["metallic"] = overrides.metallic;
                        componentJson["overrideRoughness"] = overrides.overrideRoughness;
                        componentJson["roughness"] = overrides.roughness;
                        componentJson["overrideReflectionStrength"] = overrides.overrideReflectionStrength;
                        componentJson["reflectionStrength"] = overrides.reflectionStrength;
                        componentJson["overrideOpacity"] = overrides.overrideOpacity;
                        componentJson["opacity"] = overrides.opacity;
                        componentJson["overrideAlphaCutoff"] = overrides.overrideAlphaCutoff;
                        componentJson["alphaCutoff"] = overrides.alphaCutoff;
                        componentJson["overrideDoubleSided"] = overrides.overrideDoubleSided;
                        componentJson["doubleSided"] = overrides.doubleSided;
                        componentJson["overrideUVTransform"] = overrides.overrideUVTransform;
                        componentJson["uvScale"] = {overrides.uvScale.x, overrides.uvScale.y};
                        componentJson["uvOffset"] = {overrides.uvOffset.x, overrides.uvOffset.y};

                        componentJson["overrideBlendMode"] = overrides.overrideBlendMode;
                        switch (overrides.blendMode) {
                            case BlendMode::Opaque:   componentJson["blendMode"] = "Opaque"; break;
                            case BlendMode::Alpha:    componentJson["blendMode"] = "Alpha"; break;
                            case BlendMode::Additive: componentJson["blendMode"] = "Additive"; break;
                        }

                        // Absent means "keep what the model loaded".
                        if (!overrides.diffuseTexturePath.empty()) {
                            componentJson["diffuseTexture"] = overrides.diffuseTexturePath;
                        }
                        if (!overrides.normalTexturePath.empty()) {
                            componentJson["normalTexture"] = overrides.normalTexturePath;
                        }
                        if (!overrides.armTexturePath.empty()) {
                            componentJson["armTexture"] = overrides.armTexturePath;
                        }

                        if (!overrides.shaderVertexPathLinux.empty() && !overrides.shaderFragmentPathLinux.empty()) {
                            componentJson["shaderVertexPathLinux"] = overrides.shaderVertexPathLinux;
                            componentJson["shaderFragmentPathLinux"] = overrides.shaderFragmentPathLinux;
                        }
                        if (!overrides.shaderVertexPathVita.empty() && !overrides.shaderFragmentPathVita.empty()) {
                            componentJson["shaderVertexPathVita"] = overrides.shaderVertexPathVita;
                            componentJson["shaderFragmentPathVita"] = overrides.shaderFragmentPathVita;
                        }
                        if (!overrides.shaderVertexPathVulkan.empty() && !overrides.shaderFragmentPathVulkan.empty()) {
                            componentJson["shaderVertexPathVulkan"] = overrides.shaderVertexPathVulkan;
                            componentJson["shaderFragmentPathVulkan"] = overrides.shaderFragmentPathVulkan;
                        }
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
                        componentJson["constant"] = lightComp->getConstant();
                        componentJson["linear"] = lightComp->getLinear();
                        componentJson["quadratic"] = lightComp->getQuadratic();
                        componentJson["castShadows"] = lightComp->getCastShadows();
                        componentJson["shadowStrength"] = lightComp->getShadowStrength();
                        componentJson["shadowBias"] = lightComp->getShadowBias();
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
                            case CollisionShapeType::RAMP: shapeType = "RAMP"; break;
                            case CollisionShapeType::PLANE: shapeType = "PLANE"; break;
                        }
                        componentJson["collisionShapeType"] = shapeType;
                        glm::vec3 dims = physicsComp->getShapeDimensions();
                        componentJson["shapeDimensions"] = {dims.x, dims.y, dims.z};
                        
                        // Body type
                        std::string bodyType = "STATIC";
                        switch (physicsComp->getBodyType()) {
                            case PhysicsBodyType::STATIC: bodyType = "STATIC"; break;
                            case PhysicsBodyType::DYNAMIC: bodyType = "DYNAMIC"; break;
                            case PhysicsBodyType::KINEMATIC: bodyType = "KINEMATIC"; break;
                        }
                        componentJson["bodyType"] = bodyType;
                        
                        componentJson["gravityEnabled"] = physicsComp->isGravityEnabled();
                        
                        if (physicsComp->getCollisionFilterGroup() != -1)
                            componentJson["collisionGroup"] = physicsComp->getCollisionFilterGroup();
                        if (physicsComp->getCollisionFilterMask() != -1)
                            componentJson["collisionMask"] = physicsComp->getCollisionFilterMask();
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
                } else if (component->getTypeName() == "RaycastComponent") {
                    auto raycastComp = node->getComponent<RaycastComponent>();
                    if (raycastComp) {
                        glm::vec3 from = raycastComp->getFrom();
                        glm::vec3 to = raycastComp->getTo();
                        componentJson["from"] = {from.x, from.y, from.z};
                        componentJson["to"] = {to.x, to.y, to.z};
                        componentJson["collisionMask"] = raycastComp->getCollisionMask();
                        componentJson["showDebugLine"] = raycastComp->getShowDebugLine();
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
                } else if (component->getTypeName() == "SoundComponent") {
                    auto soundComp = node->getComponent<SoundComponent>();
                    if (soundComp) {
                        componentJson["soundFile"] = soundComp->getSoundFile();
                        componentJson["volume"] = soundComp->getVolume();
                        componentJson["loop"] = soundComp->isLooping();
                    }
                } else if (component->getTypeName() == "SkyboxComponent") {
                    auto skyboxComp = node->getComponent<SkyboxComponent>();
                    if (skyboxComp) {
                        componentJson["active"] = skyboxComp->isActive();
                        auto texturePaths = skyboxComp->getTexturePaths();
                        if (texturePaths.size() >= 6) {
                            componentJson["rightTexture"] = texturePaths[0];
                            componentJson["leftTexture"] = texturePaths[1];
                            componentJson["topTexture"] = texturePaths[2];
                            componentJson["bottomTexture"] = texturePaths[3];
                            componentJson["frontTexture"] = texturePaths[4];
                            componentJson["backTexture"] = texturePaths[5];
                        }
                    }
                } else if (component->getTypeName() == "JointComponent") {
                    auto jointComp = node->getComponent<JointComponent>();
                    if (jointComp) {
                        componentJson["bodyA"] = jointComp->getBodyA();
                        componentJson["bodyB"] = jointComp->getBodyB();
                        componentJson["jointType"] = "FIXED";
                        glm::vec3 pa = jointComp->getPivotA();
                        componentJson["pivotA"] = json::array({ pa.x, pa.y, pa.z });
                        glm::vec3 pb = jointComp->getPivotB();
                        componentJson["pivotB"] = json::array({ pb.x, pb.y, pb.z });
                        componentJson["enabled"] = jointComp->isEnabled();
                    }
                } else if (component->getTypeName() == "NavObstacleComponent") {
                    // no extra props
                } else if (component->getTypeName() == "NavAgentComponent") {
                    auto navAgent = node->getComponent<NavAgentComponent>();
                    if (navAgent) {
                        componentJson["speed"] = navAgent->getSpeed();
                        componentJson["turnSpeed"] = navAgent->getTurnSpeed();
                        if (!navAgent->getAssignedVolumeNodeName().empty())
                            componentJson["assignedVolumeNodeName"] = navAgent->getAssignedVolumeNodeName();
                    }
                } else if (component->getTypeName() == "NavVolumeComponent") {
                    auto navVol = node->getComponent<NavVolumeComponent>();
                    if (navVol) {
                        componentJson["gridSizeX"] = navVol->getGridSizeX();
                        componentJson["gridSizeZ"] = navVol->getGridSizeZ();
                        componentJson["cellSize"] = navVol->getCellSize();
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
                            mesh = Mesh::getPrimitive(MeshType::QUAD);
                        } else if (meshType == "PLANE") {
                            mesh = Mesh::getPrimitive(MeshType::PLANE);
                        } else if (meshType == "CUBE") {
                            mesh = Mesh::getPrimitive(MeshType::CUBE);
                        } else if (meshType == "SPHERE") {
                            mesh = Mesh::getPrimitive(MeshType::SPHERE);
                        } else if (meshType == "CAPSULE") {
                            mesh = Mesh::getPrimitive(MeshType::CAPSULE);
                        } else if (meshType == "CYLINDER") {
                            mesh = Mesh::getPrimitive(MeshType::CYLINDER);
                        } else if (meshType == "RAMP") {
                            mesh = Mesh::getPrimitive(MeshType::RAMP);
                        } else if (meshType == "LINE") {
                            mesh = Mesh::getPrimitive(MeshType::LINE);
                        } else {
                            mesh = Mesh::getPrimitive(MeshType::CUBE);
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
                        
                        if (materialJson.contains("metallic")) {
                            material->setMetallic(materialJson["metallic"]);
                        }
                        if (materialJson.contains("roughness")) {
                            material->setRoughness(materialJson["roughness"]);
                        }
                        if (materialJson.contains("reflectionStrength")) {
                            material->setReflectionStrength(materialJson["reflectionStrength"]);
                        }
                        if (materialJson.contains("blendMode")) {
                            std::string bm = materialJson["blendMode"];
                            if (bm == "Alpha") material->setBlendMode(BlendMode::Alpha);
                            else if (bm == "Additive") material->setBlendMode(BlendMode::Additive);
                            else material->setBlendMode(BlendMode::Opaque);
                        }
                        if (materialJson.contains("opacity")) {
                            material->setOpacity(materialJson["opacity"]);
                        }
                        if (materialJson.contains("depthWrite")) {
                            material->setDepthWrite(materialJson["depthWrite"]);
                        }
                        if (materialJson.contains("uvScale") && materialJson["uvScale"].is_array() && materialJson["uvScale"].size() >= 2) {
                            auto uvScale = materialJson["uvScale"];
                            material->setUVScale(glm::vec2(uvScale[0], uvScale[1]));
                        }
                        if (materialJson.contains("uvOffset") && materialJson["uvOffset"].is_array() && materialJson["uvOffset"].size() >= 2) {
                            auto uvOffset = materialJson["uvOffset"];
                            material->setUVOffset(glm::vec2(uvOffset[0], uvOffset[1]));
                        }
                        
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
                        
                        bool loadedLinuxPaths = false;
                        bool loadedVitaPaths = false;
                        
                        if (materialJson.contains("shaderVertexPathLinux") && materialJson.contains("shaderFragmentPathLinux")) {
                            std::string linuxVertexPath = materialJson["shaderVertexPathLinux"];
                            std::string linuxFragmentPath = materialJson["shaderFragmentPathLinux"];
                            if (!linuxVertexPath.empty() && !linuxFragmentPath.empty()) {
                                material->setShaderFromPathsForPlatform(linuxVertexPath, linuxFragmentPath, "linux");
                                loadedLinuxPaths = true;
                            }
                        }
                        
                        if (materialJson.contains("shaderVertexPathVita") && materialJson.contains("shaderFragmentPathVita")) {
                            std::string vitaVertexPath = materialJson["shaderVertexPathVita"];
                            std::string vitaFragmentPath = materialJson["shaderFragmentPathVita"];
                            if (!vitaVertexPath.empty() && !vitaFragmentPath.empty()) {
                                material->setShaderFromPathsForPlatform(vitaVertexPath, vitaFragmentPath, "vita");
                                loadedVitaPaths = true;
                            }
                        }

                        if (materialJson.contains("shaderVertexPathVulkan") && materialJson.contains("shaderFragmentPathVulkan")) {
                            std::string vulkanVertexPath = materialJson["shaderVertexPathVulkan"];
                            std::string vulkanFragmentPath = materialJson["shaderFragmentPathVulkan"];
                            if (!vulkanVertexPath.empty() && !vulkanFragmentPath.empty()) {
                                material->setShaderFromPathsForPlatform(vulkanVertexPath, vulkanFragmentPath, "vulkan");
                            }
                        }
                        
                        if (!loadedLinuxPaths && !loadedVitaPaths && 
                            materialJson.contains("shaderVertexPath") && materialJson.contains("shaderFragmentPath")) {
                            std::string vertexPath = materialJson["shaderVertexPath"];
                            std::string fragmentPath = materialJson["shaderFragmentPath"];
                            if (!vertexPath.empty() && !fragmentPath.empty()) {
                                if (vertexPath.find("linux_shaders") != std::string::npos || 
                                    fragmentPath.find("linux_shaders") != std::string::npos) {
                                    material->setShaderFromPathsForPlatform(vertexPath, fragmentPath, "linux");
                                } else if (vertexPath.find("assets/vulkan") != std::string::npos ||
                                           fragmentPath.find("assets/vulkan") != std::string::npos) {
                                    material->setShaderFromPathsForPlatform(vertexPath, fragmentPath, "vulkan");
                                } else {
                                    material->setShaderFromPathsForPlatform(vertexPath, fragmentPath, "vita");
                                }
                            }
                        }
                        
                        if (materialJson.contains("customTextureUniforms") && materialJson["customTextureUniforms"].is_array()) {
                            for (const auto& entry : materialJson["customTextureUniforms"]) {
                                if (entry.contains("name") && entry.contains("path")) {
                                    std::string name = entry["name"];
                                    std::string path = entry["path"];
                                    if (!name.empty() && !path.empty()) {
                                        material->addCustomTextureUniform(name, path);
                                    }
                                }
                            }
                        }
                        
                        auto& shaderManager = ShaderManager::getInstance();
                        if (material->getShader() && material->getShader()->isValid()) {
                            shaderManager.registerShaderType(material->getShader(), ShaderType::Lit);
                        }
                        
                        meshRenderer->setMaterial(material);
                    }
                } else if (type == "BeamRenderer") {
                    auto beamRenderer = node->addComponent<BeamRenderer>();
                    if (componentJson.contains("beamWidth")) {
                        beamRenderer->setBeamWidth(componentJson["beamWidth"]);
                    }
                    if (componentJson.contains("material")) {
                        auto material = std::make_shared<Material>();
                        auto& materialJson = componentJson["material"];
                        if (materialJson.contains("color") && materialJson["color"].is_array() && materialJson["color"].size() >= 3) {
                            auto c = materialJson["color"];
                            material->setColor(glm::vec3(c[0], c[1], c[2]));
                        }
                        if (materialJson.contains("metallic")) material->setMetallic(materialJson["metallic"]);
                        if (materialJson.contains("roughness")) material->setRoughness(materialJson["roughness"]);
                        if (materialJson.contains("reflectionStrength")) material->setReflectionStrength(materialJson["reflectionStrength"]);
                        if (materialJson.contains("blendMode")) {
                            std::string bm = materialJson["blendMode"];
                            if (bm == "Alpha") material->setBlendMode(BlendMode::Alpha);
                            else if (bm == "Additive") material->setBlendMode(BlendMode::Additive);
                            else material->setBlendMode(BlendMode::Opaque);
                        }
                        if (materialJson.contains("opacity")) {
                            material->setOpacity(materialJson["opacity"]);
                        }
                        if (materialJson.contains("depthWrite")) {
                            material->setDepthWrite(materialJson["depthWrite"]);
                        }
                        if (materialJson.contains("uvScale") && materialJson["uvScale"].is_array() && materialJson["uvScale"].size() >= 2) {
                            auto uvScale = materialJson["uvScale"];
                            material->setUVScale(glm::vec2(uvScale[0], uvScale[1]));
                        }
                        if (materialJson.contains("uvOffset") && materialJson["uvOffset"].is_array() && materialJson["uvOffset"].size() >= 2) {
                            auto uvOffset = materialJson["uvOffset"];
                            material->setUVOffset(glm::vec2(uvOffset[0], uvOffset[1]));
                        }
                        bool loadedLinux = false, loadedVita = false;
                        if (materialJson.contains("shaderVertexPathLinux") && materialJson.contains("shaderFragmentPathLinux")) {
                            std::string v = materialJson["shaderVertexPathLinux"], f = materialJson["shaderFragmentPathLinux"];
                            if (!v.empty() && !f.empty()) {
                                material->setShaderFromPathsForPlatform(v, f, "linux");
                                loadedLinux = true;
                            }
                        }
                        if (materialJson.contains("shaderVertexPathVita") && materialJson.contains("shaderFragmentPathVita")) {
                            std::string v = materialJson["shaderVertexPathVita"], f = materialJson["shaderFragmentPathVita"];
                            if (!v.empty() && !f.empty()) {
                                material->setShaderFromPathsForPlatform(v, f, "vita");
                                loadedVita = true;
                            }
                        }
                        if (materialJson.contains("shaderVertexPathVulkan") && materialJson.contains("shaderFragmentPathVulkan")) {
                            std::string v = materialJson["shaderVertexPathVulkan"], f = materialJson["shaderFragmentPathVulkan"];
                            if (!v.empty() && !f.empty()) {
                                material->setShaderFromPathsForPlatform(v, f, "vulkan");
                            }
                        }
                        if (!loadedLinux) {
                            material->setShaderFromPathsForPlatform("assets/linux_shaders/beam.vert", "assets/linux_shaders/beam.frag", "linux");
                        }
                        if (!loadedVita) {
                            material->setShaderFromPathsForPlatform("assets/shaders/beam.vert", "assets/shaders/beam.frag", "vita");
                        }
                        if (materialJson.contains("customTextureUniforms") && materialJson["customTextureUniforms"].is_array()) {
                            for (const auto& entry : materialJson["customTextureUniforms"]) {
                                if (entry.contains("name") && entry.contains("path")) {
                                    std::string name = entry["name"];
                                    std::string path = entry["path"];
                                    if (!name.empty() && !path.empty()) {
                                        material->addCustomTextureUniform(name, path);
                                    }
                                }
                            }
                        }
                        beamRenderer->setMaterial(material);
                    }
                } else if (type == "ModelRenderer") {
                    auto modelRenderer = node->addComponent<ModelRenderer>();
                    
                    if (componentJson.contains("modelPath")) {
                        std::string modelPath = AssetPaths::toPortable(componentJson["modelPath"]);
                        modelRenderer->loadModel(modelPath);
                    }
                    
                    if (componentJson.contains("castShadows")) {
                        modelRenderer->setCastShadows(componentJson["castShadows"]);
                    }
                    
                    if (componentJson.contains("receiveShadows")) {
                        modelRenderer->setReceiveShadows(componentJson["receiveShadows"]);
                    }
                } else if (type == "MaterialComponent") {
                    auto materialComponent = node->addComponent<MaterialComponent>();
                    MaterialOverride overrides;

                    overrides.overrideBaseColor = componentJson.value("overrideBaseColor", false);
                    if (componentJson.contains("baseColor") && componentJson["baseColor"].is_array()
                        && componentJson["baseColor"].size() >= 3) {
                        auto baseColor = componentJson["baseColor"];
                        overrides.baseColor = glm::vec3(baseColor[0], baseColor[1], baseColor[2]);
                    }

                    overrides.overrideMetallic = componentJson.value("overrideMetallic", false);
                    overrides.metallic = componentJson.value("metallic", overrides.metallic);
                    overrides.overrideRoughness = componentJson.value("overrideRoughness", false);
                    overrides.roughness = componentJson.value("roughness", overrides.roughness);
                    overrides.overrideReflectionStrength = componentJson.value("overrideReflectionStrength", false);
                    overrides.reflectionStrength = componentJson.value("reflectionStrength", overrides.reflectionStrength);
                    overrides.overrideOpacity = componentJson.value("overrideOpacity", false);
                    overrides.opacity = componentJson.value("opacity", overrides.opacity);
                    overrides.overrideAlphaCutoff = componentJson.value("overrideAlphaCutoff", false);
                    overrides.alphaCutoff = componentJson.value("alphaCutoff", overrides.alphaCutoff);
                    overrides.overrideDoubleSided = componentJson.value("overrideDoubleSided", false);
                    overrides.doubleSided = componentJson.value("doubleSided", overrides.doubleSided);

                    overrides.overrideBlendMode = componentJson.value("overrideBlendMode", false);
                    if (componentJson.contains("blendMode")) {
                        std::string blendMode = componentJson["blendMode"];
                        if (blendMode == "Alpha") overrides.blendMode = BlendMode::Alpha;
                        else if (blendMode == "Additive") overrides.blendMode = BlendMode::Additive;
                        else overrides.blendMode = BlendMode::Opaque;
                    }

                    overrides.overrideUVTransform = componentJson.value("overrideUVTransform", false);
                    if (componentJson.contains("uvScale") && componentJson["uvScale"].is_array()
                        && componentJson["uvScale"].size() >= 2) {
                        auto uvScale = componentJson["uvScale"];
                        overrides.uvScale = glm::vec2(uvScale[0], uvScale[1]);
                    }
                    if (componentJson.contains("uvOffset") && componentJson["uvOffset"].is_array()
                        && componentJson["uvOffset"].size() >= 2) {
                        auto uvOffset = componentJson["uvOffset"];
                        overrides.uvOffset = glm::vec2(uvOffset[0], uvOffset[1]);
                    }

                    overrides.diffuseTexturePath = componentJson.value("diffuseTexture", std::string());
                    overrides.normalTexturePath = componentJson.value("normalTexture", std::string());
                    overrides.armTexturePath = componentJson.value("armTexture", std::string());

                    overrides.shaderVertexPathLinux = componentJson.value("shaderVertexPathLinux", std::string());
                    overrides.shaderFragmentPathLinux = componentJson.value("shaderFragmentPathLinux", std::string());
                    overrides.shaderVertexPathVita = componentJson.value("shaderVertexPathVita", std::string());
                    overrides.shaderFragmentPathVita = componentJson.value("shaderFragmentPathVita", std::string());
                    overrides.shaderVertexPathVulkan = componentJson.value("shaderVertexPathVulkan", std::string());
                    overrides.shaderFragmentPathVulkan = componentJson.value("shaderFragmentPathVulkan", std::string());
                    materialComponent->setOverrides(overrides);
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
                    
                    if (componentJson.contains("constant")) {
                        lightComp->setConstant(componentJson["constant"]);
                    }
                    if (componentJson.contains("linear")) {
                        lightComp->setLinear(componentJson["linear"]);
                    }
                    if (componentJson.contains("quadratic")) {
                        lightComp->setQuadratic(componentJson["quadratic"]);
                    }

                    if (componentJson.contains("castShadows")) {
                        lightComp->setCastShadows(componentJson["castShadows"]);
                    }
                    if (componentJson.contains("shadowStrength")) {
                        lightComp->setShadowStrength(componentJson["shadowStrength"]);
                    }
                    if (componentJson.contains("shadowBias")) {
                        lightComp->setShadowBias(componentJson["shadowBias"]);
                    }

#ifndef VITA_BUILD
                    lightComp->start(); // Initialize the light
#endif
                } else if (type == "PhysicsComponent") {
                    auto physicsComp = node->addComponent<PhysicsComponent>();
                    
                    if (componentJson.contains("collisionShapeType")) {
                        std::string shapeType = componentJson["collisionShapeType"];
                        glm::vec3 dims(1.0f);
                        if (componentJson.contains("shapeDimensions") && componentJson["shapeDimensions"].is_array() && componentJson["shapeDimensions"].size() >= 3) {
                            auto& sd = componentJson["shapeDimensions"];
                            dims = glm::vec3(sd[0], sd[1], sd[2]);
                        }
                        if (shapeType == "BOX") {
                            physicsComp->setCollisionShape(CollisionShapeType::BOX, dims);
                        } else if (shapeType == "SPHERE") {
                            physicsComp->setCollisionShape(CollisionShapeType::SPHERE, dims);
                        } else if (shapeType == "CAPSULE") {
                            physicsComp->setCollisionShape(CollisionShapeType::CAPSULE, dims);
                        } else if (shapeType == "CYLINDER") {
                            physicsComp->setCollisionShape(CollisionShapeType::CYLINDER, dims);
                        } else if (shapeType == "RAMP") {
                            physicsComp->setCollisionShape(CollisionShapeType::RAMP, dims);
                        } else if (shapeType == "PLANE") {
                            physicsComp->setCollisionShape(CollisionShapeType::PLANE, dims);
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
                    
                    if (componentJson.contains("gravityEnabled")) {
                        physicsComp->setGravityEnabled(componentJson["gravityEnabled"]);
                    }
                    
                    if (componentJson.contains("collisionGroup")) {
                        physicsComp->setCollisionFilterGroup(componentJson["collisionGroup"]);
                    }
                    if (componentJson.contains("collisionMask")) {
                        physicsComp->setCollisionFilterMask(componentJson["collisionMask"]);
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
                    
#ifndef VITA_BUILD
                    textComp->start(); // Initialize the text component
#endif
                } else if (type == "ScriptComponent") {
                    auto scriptComp = node->addComponent<ScriptComponent>();
                    
                    std::string scriptPath = "";
                    if (componentJson.contains("scriptPath") && !componentJson["scriptPath"].is_null()) {
                        scriptPath = componentJson["scriptPath"];
                    } else if (componentJson.contains("script") && !componentJson["script"].is_null()) {
                        scriptPath = componentJson["script"];
                    }
                    
                    if (!scriptPath.empty()) {
                        scriptComp->assignScriptPath(scriptPath);
                    } else {
#ifdef LINUX_BUILD
                        std::cout << "ScriptComponent on node \"" << name << "\" has empty script path, skipping start()" << std::endl;
#endif
                    }
                    
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
                } else if (type == "SoundComponent") {
                    auto soundComp = node->addComponent<SoundComponent>();
                    
                    if (componentJson.contains("volume")) {
                        soundComp->setVolume(componentJson["volume"]);
                    }
                    
                    if (componentJson.contains("loop")) {
                        soundComp->setLoop(componentJson["loop"]);
                    }
                    
                    if (componentJson.contains("soundFile") && !componentJson["soundFile"].is_null()) {
                        soundComp->setSoundFile(componentJson["soundFile"]);
                    }
                    
#ifndef VITA_BUILD
                    soundComp->start();
#endif
                } else if (type == "SkyboxComponent") {
                    auto skyboxComp = node->addComponent<SkyboxComponent>();
                    
                    if (componentJson.contains("rightTexture")) {
                        skyboxComp->setRightTexture(componentJson["rightTexture"]);
                    }
                    if (componentJson.contains("leftTexture")) {
                        skyboxComp->setLeftTexture(componentJson["leftTexture"]);
                    }
                    if (componentJson.contains("topTexture")) {
                        skyboxComp->setTopTexture(componentJson["topTexture"]);
                    }
                    if (componentJson.contains("bottomTexture")) {
                        skyboxComp->setBottomTexture(componentJson["bottomTexture"]);
                    }
                    if (componentJson.contains("frontTexture")) {
                        skyboxComp->setFrontTexture(componentJson["frontTexture"]);
                    }
                    if (componentJson.contains("backTexture")) {
                        skyboxComp->setBackTexture(componentJson["backTexture"]);
                    }
                    
#ifndef VITA_BUILD
                    skyboxComp->start();
#endif
                    
                    if (componentJson.contains("active")) {
                        bool active = componentJson["active"];
                        skyboxComp->setActive(active);
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
                    
#ifndef VITA_BUILD
                    area3DComp->start(); // Initialize the area3D component
#endif
                } else if (type == "RaycastComponent") {
                    auto raycastComp = node->addComponent<RaycastComponent>();
                    if (componentJson.contains("from") && componentJson["from"].is_array() && componentJson["from"].size() >= 3) {
                        auto& f = componentJson["from"];
                        raycastComp->setFrom(glm::vec3(f[0], f[1], f[2]));
                    }
                    if (componentJson.contains("to") && componentJson["to"].is_array() && componentJson["to"].size() >= 3) {
                        auto& t = componentJson["to"];
                        raycastComp->setTo(glm::vec3(t[0], t[1], t[2]));
                    }
                    if (componentJson.contains("collisionMask")) {
                        raycastComp->setCollisionMask(componentJson["collisionMask"]);
                    }
                    if (componentJson.contains("showDebugLine")) {
                        raycastComp->setShowDebugLine(componentJson["showDebugLine"]);
                    }
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
                } else if (type == "JointComponent") {
                    auto jointComp = node->addComponent<JointComponent>();
                    if (componentJson.contains("bodyA")) {
                        jointComp->setBodyA(componentJson["bodyA"]);
                    }
                    if (componentJson.contains("bodyB")) {
                        jointComp->setBodyB(componentJson["bodyB"]);
                    }
                    if (componentJson.contains("pivotA") && componentJson["pivotA"].is_array() && componentJson["pivotA"].size() >= 3) {
                        auto& pa = componentJson["pivotA"];
                        jointComp->setPivotA(glm::vec3(pa[0], pa[1], pa[2]));
                    }
                    if (componentJson.contains("pivotB") && componentJson["pivotB"].is_array() && componentJson["pivotB"].size() >= 3) {
                        auto& pb = componentJson["pivotB"];
                        jointComp->setPivotB(glm::vec3(pb[0], pb[1], pb[2]));
                    }
                    if (componentJson.contains("enabled")) {
                        jointComp->setEnabled(componentJson["enabled"]);
                    }
                } else if (type == "NavObstacleComponent") {
#ifndef VITA_BUILD
                    node->addComponent<NavObstacleComponent>()->start();
#else
                    node->addComponent<NavObstacleComponent>();
#endif
                } else if (type == "NavAgentComponent") {
                    auto navAgent = node->addComponent<NavAgentComponent>();
                    if (componentJson.contains("speed")) {
                        navAgent->setSpeed(componentJson["speed"]);
                    }
                    if (componentJson.contains("turnSpeed")) {
                        navAgent->setTurnSpeed(componentJson["turnSpeed"]);
                    }
                    if (componentJson.contains("assignedVolumeNodeName") && componentJson["assignedVolumeNodeName"].is_string()) {
                        navAgent->setAssignedVolumeNodeName(componentJson["assignedVolumeNodeName"].get<std::string>());
                    }
                } else if (type == "NavVolumeComponent") {
                    auto navVol = node->addComponent<NavVolumeComponent>();
                    if (componentJson.contains("gridSizeX")) navVol->setGridSizeX(componentJson["gridSizeX"]);
                    if (componentJson.contains("gridSizeZ")) navVol->setGridSizeZ(componentJson["gridSizeZ"]);
                    if (componentJson.contains("cellSize")) navVol->setCellSize(componentJson["cellSize"]);
#ifndef VITA_BUILD
                    navVol->start();
#endif
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

std::shared_ptr<SceneNode> SceneSerializer::duplicateNodeSubtree(std::shared_ptr<SceneNode> node) {
    if (!node) return nullptr;
    nlohmann::json nodeJson = serializeNodeToJson(node);
    return deserializeNodeFromJson(nodeJson);
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
        // Check if config/input_mappings.txt exists
        std::ifstream inputFile("config/input_mappings.txt");
        if (!inputFile.is_open()) {
            std::cout << "No config/input_mappings.txt found, creating default one..." << std::endl;
            
            std::filesystem::create_directories("config");
            
            // Create default input mappings file
            std::ofstream defaultFile("config/input_mappings.txt");
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
                std::cout << "Created default config/input_mappings.txt" << std::endl;
            }
        } else {
            inputFile.close();
            std::cout << "Found existing config/input_mappings.txt" << std::endl;
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
    std::ifstream inputFile("config/input_mappings.txt");
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
    std::ofstream manifestFile("config/input_mappings.txt");
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
        std::string scriptsDir = "assets/scripts";
        
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
        newVpkCommand += "\t\t-a config/input_mappings.txt=config/input_mappings.txt \\\n";
        newVpkCommand += "\t\t-a config/shadow_settings.txt=config/shadow_settings.txt \\\n";
        
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
            // e.g., assets/scripts/player_behavior.lua -> assets/scripts/player_behavior.lua
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
    // VPK-relative: everything below the asset root, so
    // /home/user/project/assets/models/well.glb becomes models/well.glb
    const std::string portable = AssetPaths::toPortable(path);
    const std::string assetsPrefix = AssetPaths::getAssetRoot() + "/";
    if (portable.compare(0, assetsPrefix.length(), assetsPrefix) == 0) {
        return portable.substr(assetsPrefix.length());
    }
    return portable;
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
