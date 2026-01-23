#include "Components/ModelRenderer.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureManager.h"
#include "Rendering/AnimationManager.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Scene/SceneNode.h"
#include "Components/AnimationComponent.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#ifdef LINUX_BUILD
    #include <filesystem>
#else
    #include <psp2/io/fcntl.h>
    #include <psp2/io/stat.h>
#endif

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

// Include BinaryModel for Vita builds
#include "Rendering/BinaryModel.h"

// Include tinygltf for GLTF loading
#include "../../vendor/tinygltf/tiny_gltf.h"
#include "../../vendor/tinygltf/stb_image.h"

namespace GameEngine {

ModelRenderer::ModelRenderer()
    : castShadows(true)
    , receiveShadows(true)
{
    // Initialize model data
    modelData.isLoaded = false;
    
    // Constructor debug output removed
}

ModelRenderer::~ModelRenderer() {
    unloadModel();
}

void ModelRenderer::render(Renderer& renderer) {
    if (!modelData.isLoaded || !owner) {
        return;
    }
    
    // Get the world transform matrix from the scene node
    glm::mat4 sceneNodeMatrix = owner->getWorldMatrix();
    
    auto animComp = owner->getComponent<AnimationComponent>();
    if (!animComp && owner->getParent()) {
        animComp = owner->getParent()->getComponent<AnimationComponent>();
    }
    
    std::vector<glm::mat4> boneTransforms;
    if (animComp) {
        boneTransforms = animComp->getBoneTransforms();
    }
    
    // Render each mesh in the model
    for (size_t i = 0; i < modelData.meshes.size(); ++i) {
        auto mesh = modelData.meshes[i];
        
        if (!mesh) continue;
        
        std::shared_ptr<Material> material = nullptr;
        if (i < modelData.meshMaterialIndices.size()) {
            int materialIndex = modelData.meshMaterialIndices[i];
            if (materialIndex >= 0 && materialIndex < static_cast<int>(modelData.materials.size())) {
                material = modelData.materials[materialIndex];
            }
        }
        
        glm::mat4 gltfNodeTransform = (i < modelData.meshNodeTransforms.size()) 
            ? modelData.meshNodeTransforms[i] 
            : glm::mat4(1.0f);
        glm::mat4 modelMatrix = sceneNodeMatrix * gltfNodeTransform;
        
        // Create render command
        RenderCommand command;
        command.mesh = mesh;
        command.material = material;
        command.modelMatrix = modelMatrix;
        command.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
        command.boneTransforms = boneTransforms;  // Pass bone transforms for skinning
        
        // Submit to renderer
        renderer.submitRenderCommand(command);
    }
}

bool ModelRenderer::loadModel(const std::string& modelPath) {
    if (modelPath.empty()) {
        std::cerr << "ModelRenderer: Empty model path provided" << std::endl;
        return false;
    }
    
    // Unload current model first
    unloadModel();
    
    // Convert filepath to Vita format (app0:/path for VPK files) on Vita builds
    std::string actualPath = modelPath;
#ifdef VITA_BUILD
    // Check if path already has a device prefix (app0:, ux0:, ur0:, etc.)
    if (modelPath.find("app0:") == std::string::npos && 
        modelPath.find("ux0:") == std::string::npos && 
        modelPath.find("ur0:") == std::string::npos &&
        modelPath.find("uma0:") == std::string::npos &&
        modelPath.find("imc0:") == std::string::npos &&
        modelPath.find("xmc0:") == std::string::npos &&
        modelPath.find("vs0:") == std::string::npos &&
        modelPath.find("vd0:") == std::string::npos) {
        // No device prefix found, prepend app0: for VPK access
        actualPath = "app0:/" + modelPath;
    }
#endif
    
    std::string extension = getFileExtension(actualPath);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    bool success = false;
    
    if (extension == ".gltf" || extension == ".glb") {
        success = loadGLTFModel(actualPath);
    } else if (extension == ".bmodel") {
        // Custom binary format for Vita
        success = loadBinaryModel(actualPath);
    } else {
        std::cerr << "ModelRenderer: Unsupported model format: " << extension << std::endl;
        return false;
    }
    
    if (success) {
        modelData.modelPath = modelPath;
        modelData.modelName = getFileName(modelPath);
        modelData.isLoaded = true;
    } else {
        std::cerr << "ModelRenderer: Failed to load model: " << modelPath << std::endl;
    }
    
    return success;
}

void ModelRenderer::unloadModel() {
    modelData.meshes.clear();
    modelData.materials.clear();
    modelData.meshNodeTransforms.clear();
    modelData.meshMaterialIndices.clear();
    modelData.modelPath.clear();
    modelData.modelName.clear();
    modelData.isLoaded = false;
}

glm::mat4 ModelRenderer::computeNodeTransform(const tinygltf::Node& node) {
    if (!node.matrix.empty() && node.matrix.size() == 16) {
        return glm::mat4(
            static_cast<float>(node.matrix[0]), static_cast<float>(node.matrix[1]), static_cast<float>(node.matrix[2]), static_cast<float>(node.matrix[3]),
            static_cast<float>(node.matrix[4]), static_cast<float>(node.matrix[5]), static_cast<float>(node.matrix[6]), static_cast<float>(node.matrix[7]),
            static_cast<float>(node.matrix[8]), static_cast<float>(node.matrix[9]), static_cast<float>(node.matrix[10]), static_cast<float>(node.matrix[11]),
            static_cast<float>(node.matrix[12]), static_cast<float>(node.matrix[13]), static_cast<float>(node.matrix[14]), static_cast<float>(node.matrix[15])
        );
    } else {
        glm::vec3 translation(0.0f, 0.0f, 0.0f);
        if (node.translation.size() >= 3) {
            translation = glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2])
            );
        }
        
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (node.rotation.size() >= 4) {
            rotation = glm::quat(
                static_cast<float>(node.rotation[3]),  // w
                static_cast<float>(node.rotation[0]),  // x
                static_cast<float>(node.rotation[1]),  // y
                static_cast<float>(node.rotation[2])   // z
            );
        }
        
        glm::vec3 scale(1.0f, 1.0f, 1.0f);
        if (node.scale.size() >= 3) {
            scale = glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2])
            );
        }
        
        glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
        glm::mat4 R = glm::mat4_cast(rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
        
        return T * R * S;
    }
}

void ModelRenderer::traverseGLTFNodes(const tinygltf::Model& gltfModel, const tinygltf::Node& node, 
                                      const glm::mat4& parentTransform) {
    glm::mat4 nodeTransform = computeNodeTransform(node);
    glm::mat4 worldTransform = parentTransform * nodeTransform;
    
    if (node.mesh >= 0 && node.mesh < static_cast<int>(gltfModel.meshes.size())) {
        const auto& gltfMesh = gltfModel.meshes[node.mesh];
        
        for (size_t j = 0; j < gltfMesh.primitives.size(); ++j) {
            const auto& primitive = gltfMesh.primitives[j];
            auto mesh = createMeshFromGLTF(gltfModel, gltfMesh, primitive);
            if (mesh && mesh->getVertexCount() > 0) {
                modelData.meshes.push_back(mesh);
                modelData.meshNodeTransforms.push_back(worldTransform);
                
                int materialIndex = (primitive.material >= 0 && primitive.material < static_cast<int>(gltfModel.materials.size())) 
                    ? primitive.material 
                    : -1;
                modelData.meshMaterialIndices.push_back(materialIndex);
            } else {
                std::cerr << "ModelRenderer: Failed to load primitive " << j 
                          << " from mesh '" << gltfMesh.name << "'" << std::endl;
            }
        }
    }
    
    for (int childIndex : node.children) {
        if (childIndex >= 0 && childIndex < static_cast<int>(gltfModel.nodes.size())) {
            traverseGLTFNodes(gltfModel, gltfModel.nodes[childIndex], worldTransform);
        }
    }
}

bool ModelRenderer::loadGLTFModel(const std::string& modelPath) {
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    
    bool success = false;
    std::string extension = getFileExtension(modelPath);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".glb") {
        success = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, modelPath);
    } else {
        success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, modelPath);
    }
    
    if (!success) {
        std::cerr << "ModelRenderer: Failed to load GLTF model: " << err << std::endl;
        if (!warn.empty()) {
            std::cerr << "Warnings: " << warn << std::endl;
        }
        return false;
    }
    
    
    // Extract model name early for skeleton/animation extraction
    std::string modelName = getFileName(modelPath);
    
    // Load materials first
    for (size_t i = 0; i < gltfModel.materials.size(); ++i) {
        auto material = createMaterialFromGLTF(gltfModel, i, modelPath);
        if (material) {
            modelData.materials.push_back(material);
        }
    }
    
    // Traverse scene graph starting from scene root nodes
    // Use the default scene (index 0) or the first available scene
    int sceneIndex = gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0;
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(gltfModel.scenes.size())) {
        sceneIndex = 0;
    }
    
    if (sceneIndex >= 0 && sceneIndex < static_cast<int>(gltfModel.scenes.size())) {
        const auto& scene = gltfModel.scenes[sceneIndex];
        glm::mat4 rootTransform = glm::mat4(1.0f);
        
        for (int rootNodeIndex : scene.nodes) {
            if (rootNodeIndex >= 0 && rootNodeIndex < static_cast<int>(gltfModel.nodes.size())) {
                traverseGLTFNodes(gltfModel, gltfModel.nodes[rootNodeIndex], rootTransform);
            }
        }
    } else {
        // Fallback: if no scenes, traverse all nodes that aren't children
        // This is a workaround for glTF files without scenes
        std::set<int> childNodes;
        for (const auto& node : gltfModel.nodes) {
            for (int childIndex : node.children) {
                childNodes.insert(childIndex);
            }
        }
        
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i) {
            if (childNodes.find(static_cast<int>(i)) == childNodes.end()) {
                // This is a root node
                traverseGLTFNodes(gltfModel, gltfModel.nodes[i], glm::mat4(1.0f));
            }
        }
    }
    
    // Extract skeletons from skins
    auto& animManager = AnimationManager::getInstance();
    std::string defaultSkeletonName = "";
    for (size_t i = 0; i < gltfModel.skins.size(); ++i) {
        std::string skeletonName = gltfModel.skins[i].name.empty() 
            ? modelName + "_Skeleton_" + std::to_string(i)
            : gltfModel.skins[i].name;
        
        if (i == 0) {
            defaultSkeletonName = skeletonName;
        }
        
        animManager.loadSkeleton(modelPath, skeletonName);
    }
    
    // Extract animations
    for (size_t i = 0; i < gltfModel.animations.size(); ++i) {
        std::string animName = gltfModel.animations[i].name.empty()
            ? modelName + "_Animation_" + std::to_string(i)
            : gltfModel.animations[i].name;
        
        // Use the first skeleton as target (or default name if no skeletons)
        std::string targetSkeleton = defaultSkeletonName.empty() 
            ? (modelName + "_Skeleton_0")
            : defaultSkeletonName;
        
        animManager.loadAnimationClip(modelPath, static_cast<int>(i), targetSkeleton, animName);
    }
    
    return !modelData.meshes.empty();
}

std::shared_ptr<Mesh> ModelRenderer::createMeshFromGLTF(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh, const tinygltf::Primitive& primitive) {
    auto mesh = std::make_shared<Mesh>();
    
    // Get vertex positions (required)
    if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
        std::cerr << "ModelRenderer: Primitive missing POSITION attribute" << std::endl;
        return nullptr;
    }
    
    {
        const auto& posAccessor = gltfModel.accessors[primitive.attributes.at("POSITION")];
        const auto& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
        const auto& posBuffer = gltfModel.buffers[posBufferView.buffer];
        
        const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);
        
        // Get vertex count
        size_t vertexCount = posAccessor.count;
        
        // Get normals if available
        const float* normals = nullptr;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
            const auto& normAccessor = gltfModel.accessors[primitive.attributes.at("NORMAL")];
            const auto& normBufferView = gltfModel.bufferViews[normAccessor.bufferView];
            const auto& normBuffer = gltfModel.buffers[normBufferView.buffer];
            normals = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
        }
        
        // Get texture coordinates if available
        const float* texCoords = nullptr;
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
            const auto& texAccessor = gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
            const auto& texBufferView = gltfModel.bufferViews[texAccessor.bufferView];
            const auto& texBuffer = gltfModel.buffers[texBufferView.buffer];
            texCoords = reinterpret_cast<const float*>(&texBuffer.data[texBufferView.byteOffset + texAccessor.byteOffset]);
        }
        
        // Get bone weights if available (for skinning)
        const float* boneWeights = nullptr;
        if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
            const auto& weightsAccessor = gltfModel.accessors[primitive.attributes.at("WEIGHTS_0")];
            const auto& weightsBufferView = gltfModel.bufferViews[weightsAccessor.bufferView];
            const auto& weightsBuffer = gltfModel.buffers[weightsBufferView.buffer];
            boneWeights = reinterpret_cast<const float*>(&weightsBuffer.data[weightsBufferView.byteOffset + weightsAccessor.byteOffset]);
        }
        
        // Get bone indices if available (for skinning)
        const unsigned short* boneIndices = nullptr;
        const unsigned char* boneIndicesU8 = nullptr;
        if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
            const auto& jointsAccessor = gltfModel.accessors[primitive.attributes.at("JOINTS_0")];
            const auto& jointsBufferView = gltfModel.bufferViews[jointsAccessor.bufferView];
            const auto& jointsBuffer = gltfModel.buffers[jointsBufferView.buffer];
            
            if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                boneIndices = reinterpret_cast<const unsigned short*>(&jointsBuffer.data[jointsBufferView.byteOffset + jointsAccessor.byteOffset]);
            } else if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                boneIndicesU8 = reinterpret_cast<const unsigned char*>(&jointsBuffer.data[jointsBufferView.byteOffset + jointsAccessor.byteOffset]);
            }
        }
        
        // Create vertices
        std::vector<Vertex> vertices;
        vertices.reserve(vertexCount);
        
        for (size_t i = 0; i < vertexCount; ++i) {
            Vertex vertex;
            vertex.position = glm::vec3(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
            
            if (normals) {
                vertex.normal = glm::vec3(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
            } else {
                vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default normal
            }
            
            if (texCoords) {
#ifdef VITA_BUILD
                // Vita: Don't flip texture coordinates (they're already correct)
                vertex.texCoords = glm::vec2(texCoords[i * 2], texCoords[i * 2 + 1]);
#else
                // Linux: GLTF uses flipped V coordinates compared to OpenGL
                // Flip the V coordinate to match OpenGL's texture coordinate system
                vertex.texCoords = glm::vec2(texCoords[i * 2], 1.0f - texCoords[i * 2 + 1]);
#endif
            } else {
                vertex.texCoords = glm::vec2(0.0f, 0.0f); // Default UV
            }
            
            // Load bone weights and indices for skinning
            if (boneWeights) {
                vertex.boneWeights = glm::vec4(
                    boneWeights[i * 4], 
                    boneWeights[i * 4 + 1], 
                    boneWeights[i * 4 + 2], 
                    boneWeights[i * 4 + 3]
                );
            } else {
                vertex.boneWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // Default: use first bone only
            }
            
            if (boneIndices) {
                // Convert unsigned short indices to float (shader expects float)
                vertex.boneIndices = glm::vec4(
                    static_cast<float>(boneIndices[i * 4]),
                    static_cast<float>(boneIndices[i * 4 + 1]),
                    static_cast<float>(boneIndices[i * 4 + 2]),
                    static_cast<float>(boneIndices[i * 4 + 3])
                );
            } else if (boneIndicesU8) {
                // Convert unsigned byte indices to float
                vertex.boneIndices = glm::vec4(
                    static_cast<float>(boneIndicesU8[i * 4]),
                    static_cast<float>(boneIndicesU8[i * 4 + 1]),
                    static_cast<float>(boneIndicesU8[i * 4 + 2]),
                    static_cast<float>(boneIndicesU8[i * 4 + 3])
                );
            } else {
                vertex.boneIndices = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            }
            
            vertices.push_back(vertex);
        }
        
        // Get indices if available
        std::vector<unsigned int> indices;
        if (primitive.indices >= 0) {
            const auto& indexAccessor = gltfModel.accessors[primitive.indices];
            const auto& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
            const auto& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
            
            const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
            
            for (size_t i = 0; i < indexAccessor.count; ++i) {
                unsigned int index = 0;
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    index = reinterpret_cast<const unsigned short*>(indexData)[i];
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    index = reinterpret_cast<const unsigned int*>(indexData)[i];
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    index = reinterpret_cast<const unsigned char*>(indexData)[i];
                }
                indices.push_back(index);
            }
        }
        
        // Create mesh from vertices and indices
        if (!vertices.empty()) {
            mesh->setVertices(vertices);
            if (!indices.empty()) {
                mesh->setIndices(indices);
            }
            mesh->upload();
            return mesh;
        } else {
            std::cerr << "ModelRenderer: No vertices created for mesh" << std::endl;
            return nullptr;
        }
    }
    
    std::cerr << "ModelRenderer: Failed to process mesh primitive" << std::endl;
    return nullptr;
}

std::shared_ptr<Material> ModelRenderer::createMaterialFromGLTF(const tinygltf::Model& gltfModel, int materialIndex, const std::string& modelPath) {
    if (materialIndex < 0 || materialIndex >= static_cast<int>(gltfModel.materials.size())) {
        return nullptr;
    }
    
    const auto& gltfMaterial = gltfModel.materials[materialIndex];
    auto material = std::make_shared<Material>();
    
    // Set base color
    if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() >= 3) {
        glm::vec3 color(
            gltfMaterial.pbrMetallicRoughness.baseColorFactor[0],
            gltfMaterial.pbrMetallicRoughness.baseColorFactor[1],
            gltfMaterial.pbrMetallicRoughness.baseColorFactor[2]
        );
        material->setColor(color);
    }
    
    // Load diffuse texture if available
    if (gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0) {
        const auto& textureInfo = gltfMaterial.pbrMetallicRoughness.baseColorTexture;
        const auto& texture = gltfModel.textures[textureInfo.index];
        const auto& image = gltfModel.images[texture.source];
        
        bool textureLoaded = false;
        
        // Handle embedded images (data URI)
        if (image.uri.empty() && !image.image.empty()) {
            auto embeddedTexture = std::make_shared<Texture>();
            
            if (!image.as_is && image.width > 0 && image.height > 0 && image.component > 0) {
                TextureFormat texFormat = (image.component == 3) ? TextureFormat::RGB : TextureFormat::RGBA;
                if (embeddedTexture->createFromData(image.image.data(), image.width, image.height, texFormat)) {
                    material->setDiffuseTexture(embeddedTexture);
                    textureLoaded = true;
                }
            } else if (image.as_is) {
                int width, height, channels;
                unsigned char* decodedData = stbi_load_from_memory(
                    image.image.data(), 
                    static_cast<int>(image.image.size()),
                    &width, &height, &channels, 0
                );
                if (decodedData) {
                    TextureFormat texFormat = (channels == 3) ? TextureFormat::RGB : TextureFormat::RGBA;
                    if (embeddedTexture->createFromData(decodedData, width, height, texFormat)) {
                        material->setDiffuseTexture(embeddedTexture);
                        textureLoaded = true;
                    }
                    stbi_image_free(decodedData);
                }
            }
        }
        
        if (!textureLoaded) {
            std::string texturePath;
            
            if (!image.uri.empty()) {
                if (image.uri.substr(0, 4) == "data") {
                    texturePath = "assets/textures/default_diffuse.png";
                } else {
                    std::string modelDir = modelPath.substr(0, modelPath.find_last_of('/') + 1);
                    texturePath = modelDir + image.uri;
                }
            } else {
                texturePath = "assets/textures/default_diffuse.png";
            }
            
            auto textureManager = &TextureManager::getInstance();
            auto diffuseTexture = textureManager->getTexture(texturePath);
            if (diffuseTexture) {
                material->setDiffuseTexture(diffuseTexture);
            }
        }
    }
    
    // Load normal texture if available
    if (gltfMaterial.normalTexture.index >= 0) {
        const auto& textureInfo = gltfMaterial.normalTexture;
        const auto& texture = gltfModel.textures[textureInfo.index];
        const auto& image = gltfModel.images[texture.source];
        
        bool textureLoaded = false;
        
        // Handle embedded images (data URI)
        if (image.uri.empty() && !image.image.empty()) {
            auto embeddedTexture = std::make_shared<Texture>();
            
            if (!image.as_is && image.width > 0 && image.height > 0 && image.component > 0) {
                TextureFormat texFormat = (image.component == 3) ? TextureFormat::RGB : TextureFormat::RGBA;
                if (embeddedTexture->createFromData(image.image.data(), image.width, image.height, texFormat)) {
                    material->setNormalTexture(embeddedTexture);
                    textureLoaded = true;
                }
            } else if (image.as_is) {
                int width, height, channels;
                unsigned char* decodedData = stbi_load_from_memory(
                    image.image.data(), 
                    static_cast<int>(image.image.size()),
                    &width, &height, &channels, 0
                );
                if (decodedData) {
                    TextureFormat texFormat = (channels == 3) ? TextureFormat::RGB : TextureFormat::RGBA;
                    if (embeddedTexture->createFromData(decodedData, width, height, texFormat)) {
                        material->setNormalTexture(embeddedTexture);
                        textureLoaded = true;
                    }
                    stbi_image_free(decodedData);
                }
            }
        }
        
        if (!textureLoaded) {
            std::string texturePath;
            
            if (!image.uri.empty()) {
                if (image.uri.substr(0, 4) == "data") {
                    texturePath = "assets/textures/default_normal.png";
                } else {
                    std::string modelDir = modelPath.substr(0, modelPath.find_last_of('/') + 1);
                    texturePath = modelDir + image.uri;
                }
            } else {
                texturePath = "assets/textures/default_normal.png";
            }
            
            auto textureManager = &TextureManager::getInstance();
            auto normalTexture = textureManager->getTexture(texturePath);
            if (normalTexture) {
                material->setNormalTexture(normalTexture);
            }
        }
    }
    
    // Load ARM texture if available (Ambient Occlusion, Roughness, Metallic)
    if (gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
        const auto& textureInfo = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture;
        const auto& texture = gltfModel.textures[textureInfo.index];
        const auto& image = gltfModel.images[texture.source];
        
        bool textureLoaded = false;
        
        // Handle embedded images (data URI)
        if (image.uri.empty() && !image.image.empty()) {
            auto embeddedTexture = std::make_shared<Texture>();
            
            if (!image.as_is && image.width > 0 && image.height > 0 && image.component > 0) {
                TextureFormat texFormat = (image.component == 3) ? TextureFormat::RGB : TextureFormat::RGBA;
                if (embeddedTexture->createFromData(image.image.data(), image.width, image.height, texFormat)) {
                    material->setARMTexture(embeddedTexture);
                    textureLoaded = true;
                }
            } else if (image.as_is) {
                int width, height, channels;
                unsigned char* decodedData = stbi_load_from_memory(
                    image.image.data(), 
                    static_cast<int>(image.image.size()),
                    &width, &height, &channels, 0
                );
                if (decodedData) {
                    TextureFormat texFormat = (channels == 3) ? TextureFormat::RGB : TextureFormat::RGBA;
                    if (embeddedTexture->createFromData(decodedData, width, height, texFormat)) {
                        material->setARMTexture(embeddedTexture);
                        textureLoaded = true;
                    }
                    stbi_image_free(decodedData);
                }
            }
        }
        
        if (!textureLoaded) {
            std::string texturePath;
            
            if (!image.uri.empty()) {
                if (image.uri.substr(0, 4) == "data") {
                    texturePath = "assets/textures/default_arm.png";
                } else {
                    std::string modelDir = modelPath.substr(0, modelPath.find_last_of('/') + 1);
                    texturePath = modelDir + image.uri;
                }
            } else {
                texturePath = "assets/textures/default_arm.png";
            }
            
            auto textureManager = &TextureManager::getInstance();
            auto armTexture = textureManager->getTexture(texturePath);
            if (armTexture) {
                material->setARMTexture(armTexture);
            }
        }
    }
    
    return material;
}

bool ModelRenderer::loadBinaryModel(const std::string& modelPath) {
#ifdef VITA_BUILD
    // Use BinaryModel for Vita builds
    BinaryModel binaryModel;
    if (!binaryModel.loadFromBinary(modelPath)) {
        std::cerr << "ModelRenderer: Failed to load binary model: " << modelPath << std::endl;
        return false;
    }
    
    // Create meshes and materials from binary data
    modelData.meshes = binaryModel.createMeshes();
    modelData.materials = binaryModel.createMaterials();
    
    modelData.meshNodeTransforms.resize(modelData.meshes.size(), glm::mat4(1.0f));
    
    modelData.meshMaterialIndices.resize(modelData.meshes.size());
    for (size_t i = 0; i < modelData.meshes.size(); ++i) {
        modelData.meshMaterialIndices[i] = (i < modelData.materials.size()) ? static_cast<int>(i) : -1;
    }
    
    return !modelData.meshes.empty();
#else
    std::cerr << "ModelRenderer: Binary model loading only supported on Vita" << std::endl;
    return false;
#endif
}

bool ModelRenderer::saveBinaryModel(const std::string& modelPath) {
#ifdef VITA_BUILD
    // Convert GLTF to binary format for Vita
    BinaryModel binaryModel;
    if (!binaryModel.loadFromGLTF(modelData.modelPath)) {
        std::cerr << "ModelRenderer: Failed to convert GLTF to binary: " << modelData.modelPath << std::endl;
        return false;
    }
    
    if (!binaryModel.saveToBinary(modelPath)) {
        std::cerr << "ModelRenderer: Failed to save binary model: " << modelPath << std::endl;
        return false;
    }
    
    return true;
#else
    std::cerr << "ModelRenderer: Binary model saving only supported on Vita" << std::endl;
    return false;
#endif
}

std::vector<std::string> ModelRenderer::discoverModels(const std::string& directory) {
    std::vector<std::string> models;
    
#ifdef LINUX_BUILD
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filepath = entry.path().string();
                std::string extension = ModelRenderer::getFileExtension(filepath);
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".gltf" || extension == ".glb" || extension == ".bmodel") {
                    models.push_back(filepath);
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        std::cerr << "ModelRenderer: Error discovering models: " << ex.what() << std::endl;
    }
#endif
    
    return models;
}

void ModelRenderer::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Model Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Model: %s", modelData.isLoaded ? modelData.modelName.c_str() : "None");
        ImGui::Text("Path: %s", modelData.modelPath.c_str());
        
        if (modelData.isLoaded) {
            ImGui::Text("Meshes: %zu", modelData.meshes.size());
            ImGui::Text("Materials: %zu", modelData.materials.size());
        }
        
        ImGui::Checkbox("Cast Shadows", &castShadows);
        ImGui::Checkbox("Receive Shadows", &receiveShadows);
        
        // Model loading section
        ImGui::Separator();
        ImGui::Text("Load Model:");
        
        static char modelPathBuffer[256] = "";
        ImGui::InputText("Model Path", modelPathBuffer, sizeof(modelPathBuffer));
        
        if (ImGui::Button("Load Model")) {
            if (strlen(modelPathBuffer) > 0) {
                loadModel(std::string(modelPathBuffer));
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Browse Models")) {
            // TODO: Implement model browser dialog
            ImGui::OpenPopup("Model Browser");
        }
        
        // Model browser popup
        if (ImGui::BeginPopup("Model Browser")) {
            auto models = discoverModels("assets/models");
            for (const auto& model : models) {
                if (ImGui::Selectable(model.c_str())) {
                    loadModel(model);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        
        if (modelData.isLoaded && ImGui::Button("Unload Model")) {
            unloadModel();
        }
    }
#endif
}

std::string ModelRenderer::getFileExtension(const std::string& filepath) {
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos != std::string::npos) {
        return filepath.substr(dotPos);
    }
    return "";
}

std::string ModelRenderer::getFileName(const std::string& filepath) {
    size_t slashPos = filepath.find_last_of('/');
    if (slashPos != std::string::npos) {
        return filepath.substr(slashPos + 1);
    }
    return filepath;
}

} // namespace GameEngine
