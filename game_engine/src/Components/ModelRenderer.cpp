#include "Components/ModelRenderer.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureManager.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Scene/SceneNode.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <glm/glm.hpp>

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
    
    // Get the world transform matrix
    glm::mat4 modelMatrix = owner->getWorldMatrix();
    
    // Debug output removed for cleaner console
    
    // Render each mesh in the model
    for (size_t i = 0; i < modelData.meshes.size(); ++i) {
        auto mesh = modelData.meshes[i];
        auto material = (i < modelData.materials.size()) ? modelData.materials[i] : nullptr;
        
        if (!mesh) continue;
        
        // Create render command
        RenderCommand command;
        command.mesh = mesh;
        command.material = material;
        command.modelMatrix = modelMatrix;
        command.normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
        
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
        std::cout << "ModelRenderer: Successfully loaded model: " << modelPath << std::endl;
    } else {
        std::cerr << "ModelRenderer: Failed to load model: " << modelPath << std::endl;
    }
    
    return success;
}

void ModelRenderer::unloadModel() {
    modelData.meshes.clear();
    modelData.materials.clear();
    modelData.modelPath.clear();
    modelData.modelName.clear();
    modelData.isLoaded = false;
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
    
    if (!warn.empty()) {
        std::cout << "ModelRenderer: GLTF warnings: " << warn << std::endl;
    }
    
    // Load materials first
    for (size_t i = 0; i < gltfModel.materials.size(); ++i) {
        auto material = createMaterialFromGLTF(gltfModel, i, modelPath);
        if (material) {
            modelData.materials.push_back(material);
        }
    }
    
    // Load meshes
    for (size_t i = 0; i < gltfModel.meshes.size(); ++i) {
        const auto& gltfMesh = gltfModel.meshes[i];
        
        for (size_t j = 0; j < gltfMesh.primitives.size(); ++j) {
            const auto& primitive = gltfMesh.primitives[j];
            auto mesh = createMeshFromGLTF(gltfModel, gltfMesh, primitive);
            if (mesh) {
                modelData.meshes.push_back(mesh);
            }
        }
    }
    
    return !modelData.meshes.empty();
}

std::shared_ptr<Mesh> ModelRenderer::createMeshFromGLTF(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh, const tinygltf::Primitive& primitive) {
    auto mesh = std::make_shared<Mesh>();
    
    // Get vertex positions
    if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
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
        }
    }
    
    return mesh;
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
        
        std::string texturePath;
        
        // Handle embedded images (data URI)
        if (image.uri.empty() && !image.image.empty()) {
            // This is an embedded image - we need to save it to a temporary file
            // For now, we'll skip embedded images and use a placeholder
            std::cout << "ModelRenderer: Embedded texture detected, using placeholder" << std::endl;
            texturePath = "assets/textures/default_diffuse.png";
        } else if (!image.uri.empty()) {
            // This is a file path
            if (image.uri.substr(0, 4) == "data") {
                // Data URI - skip for now
                std::cout << "ModelRenderer: Data URI texture detected, using placeholder" << std::endl;
                texturePath = "assets/textures/default_diffuse.png";
            } else {
                // Regular file path - construct full path
                std::string modelDir = modelPath.substr(0, modelPath.find_last_of('/') + 1);
                texturePath = modelDir + image.uri;
                std::cout << "ModelRenderer: Loading diffuse texture from: " << texturePath << std::endl;
            }
        } else {
            // Fallback to placeholder
            texturePath = "assets/textures/default_diffuse.png";
        }
        
        auto textureManager = &TextureManager::getInstance();
        auto diffuseTexture = textureManager->getTexture(texturePath);
        if (diffuseTexture) {
            material->setDiffuseTexture(diffuseTexture);
            std::cout << "ModelRenderer: Successfully loaded diffuse texture: " << texturePath << std::endl;
        } else {
            std::cout << "ModelRenderer: Failed to load diffuse texture: " << texturePath << std::endl;
        }
    }
    
    // Load normal texture if available
    if (gltfMaterial.normalTexture.index >= 0) {
        const auto& textureInfo = gltfMaterial.normalTexture;
        const auto& texture = gltfModel.textures[textureInfo.index];
        const auto& image = gltfModel.images[texture.source];
        
        std::string texturePath;
        
        if (image.uri.empty() && !image.image.empty()) {
            std::cout << "ModelRenderer: Embedded normal texture detected, using placeholder" << std::endl;
            texturePath = "assets/textures/default_normal.png";
        } else if (!image.uri.empty()) {
            if (image.uri.substr(0, 4) == "data") {
                std::cout << "ModelRenderer: Data URI normal texture detected, using placeholder" << std::endl;
                texturePath = "assets/textures/default_normal.png";
            } else {
                std::string modelDir = modelPath.substr(0, modelPath.find_last_of('/') + 1);
                texturePath = modelDir + image.uri;
                std::cout << "ModelRenderer: Loading normal texture from: " << texturePath << std::endl;
            }
        } else {
            texturePath = "assets/textures/default_normal.png";
        }
        
        auto textureManager = &TextureManager::getInstance();
        auto normalTexture = textureManager->getTexture(texturePath);
        if (normalTexture) {
            material->setNormalTexture(normalTexture);
            std::cout << "ModelRenderer: Successfully loaded normal texture: " << texturePath << std::endl;
        } else {
            std::cout << "ModelRenderer: Failed to load normal texture: " << texturePath << std::endl;
        }
    }
    
    // Load ARM texture if available (Ambient Occlusion, Roughness, Metallic)
    if (gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
        const auto& textureInfo = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture;
        const auto& texture = gltfModel.textures[textureInfo.index];
        const auto& image = gltfModel.images[texture.source];
        
        std::string texturePath;
        
        if (image.uri.empty() && !image.image.empty()) {
            std::cout << "ModelRenderer: Embedded ARM texture detected, using placeholder" << std::endl;
            texturePath = "assets/textures/default_arm.png";
        } else if (!image.uri.empty()) {
            if (image.uri.substr(0, 4) == "data") {
                std::cout << "ModelRenderer: Data URI ARM texture detected, using placeholder" << std::endl;
                texturePath = "assets/textures/default_arm.png";
            } else {
                std::string modelDir = modelPath.substr(0, modelPath.find_last_of('/') + 1);
                texturePath = modelDir + image.uri;
                std::cout << "ModelRenderer: Loading ARM texture from: " << texturePath << std::endl;
            }
        } else {
            texturePath = "assets/textures/default_arm.png";
        }
        
        auto textureManager = &TextureManager::getInstance();
        auto armTexture = textureManager->getTexture(texturePath);
        if (armTexture) {
            material->setARMTexture(armTexture);
            std::cout << "ModelRenderer: Successfully loaded ARM texture: " << texturePath << std::endl;
        } else {
            std::cout << "ModelRenderer: Failed to load ARM texture: " << texturePath << std::endl;
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
    
    std::cout << "ModelRenderer: Successfully converted and saved binary model: " << modelPath << std::endl;
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
