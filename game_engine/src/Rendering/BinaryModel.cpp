#include "Rendering/BinaryModel.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/TextureManager.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

// Include tinygltf for GLTF conversion
#include "../../vendor/tinygltf/tiny_gltf.h"

namespace GameEngine {

BinaryModel::BinaryModel() : loaded(false) {
    memcpy(data.header.magic, "BMOD", 4);
    data.header.version = 1;
    data.header.meshCount = 0;
    data.header.materialCount = 0;
    data.header.totalSize = 0;
    memset(data.header.reserved, 0, 16);
}

BinaryModel::~BinaryModel() {
    clearData();
}

bool BinaryModel::loadFromGLTF(const std::string& gltfPath) {
    clearData();
    
    if (!convertGLTFToBinary(gltfPath)) {
        std::cerr << "BinaryModel: Failed to convert GLTF to binary format" << std::endl;
        return false;
    }
    
    optimizeForVita();
    loaded = true;
    
    std::cout << "BinaryModel: Successfully converted GLTF to binary format" << std::endl;
    std::cout << "  Meshes: " << data.header.meshCount << std::endl;
    std::cout << "  Materials: " << data.header.materialCount << std::endl;
    
    return true;
}

bool BinaryModel::saveToBinary(const std::string& binaryPath) {
    if (!loaded) {
        std::cerr << "BinaryModel: No data loaded to save" << std::endl;
        return false;
    }
    
    std::ofstream file(binaryPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "BinaryModel: Failed to open file for writing: " << binaryPath << std::endl;
        return false;
    }
    
    data.header.totalSize = sizeof(BinaryModelHeader);
    data.header.totalSize += data.header.meshCount * sizeof(BinaryMeshHeader);
    data.header.totalSize += data.header.materialCount * sizeof(BinaryMaterialHeader);
    
    for (const auto& mesh : data.vertexData) {
        data.header.totalSize += mesh.size() * sizeof(float);
    }
    
    for (const auto& indices : data.indexData) {
        data.header.totalSize += indices.size() * sizeof(uint16_t);
    }
    
    if (!writeHeader(file) || 
        !writeMeshData(file) || 
        !writeMaterialData(file) || 
        !writeTextureData(file)) {
        file.close();
        return false;
    }
    
    file.close();
    std::cout << "BinaryModel: Successfully saved binary model: " << binaryPath << std::endl;
    return true;
}

bool BinaryModel::loadFromBinary(const std::string& binaryPath) {
    clearData();
    
    std::ifstream file(binaryPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "BinaryModel: Failed to open file for reading: " << binaryPath << std::endl;
        return false;
    }
    
    // Read data
    if (!readHeader(file) || 
        !readMeshData(file) || 
        !readMaterialData(file) || 
        !readTextureData(file)) {
        file.close();
        return false;
    }
    
    file.close();
    loaded = true;
    
    std::cout << "BinaryModel: Successfully loaded binary model: " << binaryPath << std::endl;
    std::cout << "  Meshes: " << data.header.meshCount << std::endl;
    std::cout << "  Materials: " << data.header.materialCount << std::endl;
    
    return true;
}

std::vector<std::shared_ptr<Mesh>> BinaryModel::createMeshes() {
    std::vector<std::shared_ptr<Mesh>> meshes;
    
    if (!loaded) {
        return meshes;
    }
    
    for (size_t i = 0; i < data.meshHeaders.size(); ++i) {
        const auto& meshHeader = data.meshHeaders[i];
        const auto& vertices = data.vertexData[i];
        const auto& indices = data.indexData[i];
        
        if (vertices.empty()) continue;
        
        auto mesh = std::make_shared<Mesh>();
        
        std::vector<Vertex> meshVertices;
        meshVertices.reserve(meshHeader.vertexCount);
        
        for (size_t j = 0; j < meshHeader.vertexCount; ++j) {
            Vertex vertex;
            size_t baseIndex = j * 8; // 3 pos + 3 normal + 2 tex
            
            if (baseIndex + 7 < vertices.size()) {
                vertex.position = glm::vec3(vertices[baseIndex], vertices[baseIndex + 1], vertices[baseIndex + 2]);
                vertex.normal = glm::vec3(vertices[baseIndex + 3], vertices[baseIndex + 4], vertices[baseIndex + 5]);
                vertex.texCoords = glm::vec2(vertices[baseIndex + 6], vertices[baseIndex + 7]);
            }
            
            meshVertices.push_back(vertex);
        }
        
        std::vector<unsigned int> meshIndices;
        meshIndices.reserve(indices.size());
        for (uint16_t index : indices) {
            meshIndices.push_back(static_cast<unsigned int>(index));
        }
        
        mesh->setVertices(meshVertices);
        mesh->setIndices(meshIndices);
        mesh->upload();
        
        meshes.push_back(mesh);
    }
    
    return meshes;
}

std::vector<std::shared_ptr<Material>> BinaryModel::createMaterials() {
    std::vector<std::shared_ptr<Material>> materials;
    
    if (!loaded) {
        return materials;
    }
    
    auto& textureManager = TextureManager::getInstance();
    
    for (const auto& materialHeader : data.materialHeaders) {
        auto material = std::make_shared<Material>();
        
        glm::vec3 color(materialHeader.color[0], materialHeader.color[1], materialHeader.color[2]);
        material->setColor(color);
        
        if (materialHeader.diffuseTextureIndex < data.textureInfos.size()) {
            const auto& textureInfo = data.textureInfos[materialHeader.diffuseTextureIndex];
            auto texture = textureManager.getTexture(textureInfo.path);
            if (texture) {
                material->setDiffuseTexture(texture);
            }
        }
        
        if (materialHeader.normalTextureIndex < data.textureInfos.size()) {
            const auto& textureInfo = data.textureInfos[materialHeader.normalTextureIndex];
            auto texture = textureManager.getTexture(textureInfo.path);
            if (texture) {
                material->setNormalTexture(texture);
            }
        }
        
        if (materialHeader.armTextureIndex < data.textureInfos.size()) {
            const auto& textureInfo = data.textureInfos[materialHeader.armTextureIndex];
            auto texture = textureManager.getTexture(textureInfo.path);
            if (texture) {
                material->setARMTexture(texture, textureInfo.path);
            }
        }
        
        materials.push_back(material);
    }
    
    return materials;
}

bool BinaryModel::validateHeader(const BinaryModelHeader& header) {
    return (strncmp(header.magic, "BMOD", 4) == 0 && 
            header.version == 1 && 
            header.meshCount <= 1000 && 
            header.materialCount <= 1000);
}

void BinaryModel::clearData() {
    data.meshHeaders.clear();
    data.materialHeaders.clear();
    data.textureInfos.clear();
    data.vertexData.clear();
    data.indexData.clear();
    data.textureData.clear();
    
    data.header.meshCount = 0;
    data.header.materialCount = 0;
    data.header.totalSize = 0;
    
    loaded = false;
}

bool BinaryModel::convertGLTFToBinary(const std::string& gltfPath) {
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    
    bool success = false;
    std::string extension = gltfPath.substr(gltfPath.find_last_of('.'));
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".glb") {
        success = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, gltfPath);
    } else {
        success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, gltfPath);
    }
    
    if (!success) {
        std::cerr << "BinaryModel: Failed to load GLTF model: " << err << std::endl;
        return false;
    }
    
    data.header.materialCount = gltfModel.materials.size();
    for (const auto& gltfMaterial : gltfModel.materials) {
        BinaryMaterialHeader materialHeader;
        memset(&materialHeader, 0, sizeof(materialHeader));
        
        if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() >= 3) {
            materialHeader.color[0] = gltfMaterial.pbrMetallicRoughness.baseColorFactor[0];
            materialHeader.color[1] = gltfMaterial.pbrMetallicRoughness.baseColorFactor[1];
            materialHeader.color[2] = gltfMaterial.pbrMetallicRoughness.baseColorFactor[2];
            materialHeader.color[3] = gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() > 3 ? 
                                    gltfMaterial.pbrMetallicRoughness.baseColorFactor[3] : 1.0f;
        } else {
            materialHeader.color[0] = materialHeader.color[1] = materialHeader.color[2] = 1.0f;
            materialHeader.color[3] = 1.0f;
        }
        
        materialHeader.diffuseTextureIndex = 0xFFFFFFFF;
        materialHeader.normalTextureIndex = 0xFFFFFFFF;
        materialHeader.armTextureIndex = 0xFFFFFFFF;
        
        strncpy(materialHeader.name, gltfMaterial.name.c_str(), sizeof(materialHeader.name) - 1);
        
        data.materialHeaders.push_back(materialHeader);
    }
    
    data.header.meshCount = gltfModel.meshes.size();
    for (const auto& gltfMesh : gltfModel.meshes) {
        for (const auto& primitive : gltfMesh.primitives) {
            BinaryMeshHeader meshHeader;
            memset(&meshHeader, 0, sizeof(meshHeader));
            
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const auto& posAccessor = gltfModel.accessors[primitive.attributes.at("POSITION")];
                const auto& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
                const auto& posBuffer = gltfModel.buffers[posBufferView.buffer];
                
                const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);
                meshHeader.vertexCount = posAccessor.count;
                
                std::vector<float> vertexData;
                vertexData.reserve(meshHeader.vertexCount * 8);
                
                const float* normals = nullptr;
                if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                    const auto& normAccessor = gltfModel.accessors[primitive.attributes.at("NORMAL")];
                    const auto& normBufferView = gltfModel.bufferViews[normAccessor.bufferView];
                    const auto& normBuffer = gltfModel.buffers[normBufferView.buffer];
                    normals = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
                }
                
                const float* texCoords = nullptr;
                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                    const auto& texAccessor = gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                    const auto& texBufferView = gltfModel.bufferViews[texAccessor.bufferView];
                    const auto& texBuffer = gltfModel.buffers[texBufferView.buffer];
                    texCoords = reinterpret_cast<const float*>(&texBuffer.data[texBufferView.byteOffset + texAccessor.byteOffset]);
                }
                
                for (size_t i = 0; i < meshHeader.vertexCount; ++i) {
                    vertexData.push_back(positions[i * 3]);
                    vertexData.push_back(positions[i * 3 + 1]);
                    vertexData.push_back(positions[i * 3 + 2]);
                    
                    if (normals) {
                        vertexData.push_back(normals[i * 3]);
                        vertexData.push_back(normals[i * 3 + 1]);
                        vertexData.push_back(normals[i * 3 + 2]);
                    } else {
                        vertexData.push_back(0.0f);
                        vertexData.push_back(0.0f);
                        vertexData.push_back(1.0f);
                    }
                    
                    if (texCoords) {
                        vertexData.push_back(texCoords[i * 2]);
                        vertexData.push_back(texCoords[i * 2 + 1]);
                    } else {
                        vertexData.push_back(0.0f);
                        vertexData.push_back(0.0f);
                    }
                }
                
                data.vertexData.push_back(vertexData);
                meshHeader.vertexSize = vertexData.size() * sizeof(float);
            }
            
            if (primitive.indices >= 0) {
                const auto& indexAccessor = gltfModel.accessors[primitive.indices];
                const auto& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
                const auto& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
                
                const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
                
                std::vector<uint16_t> indices;
                indices.reserve(indexAccessor.count);
                
                for (size_t i = 0; i < indexAccessor.count; ++i) {
                    uint16_t index = 0;
                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        index = reinterpret_cast<const unsigned short*>(indexData)[i];
                    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        index = static_cast<uint16_t>(reinterpret_cast<const unsigned int*>(indexData)[i]);
                    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        index = reinterpret_cast<const unsigned char*>(indexData)[i];
                    }
                    indices.push_back(index);
                }
                
                data.indexData.push_back(indices);
                meshHeader.indexCount = indices.size();
                meshHeader.indexSize = indices.size() * sizeof(uint16_t);
            }
            
            meshHeader.materialIndex = primitive.material >= 0 ? primitive.material : 0;
            strncpy(meshHeader.name, gltfMesh.name.c_str(), sizeof(meshHeader.name) - 1);
            
            data.meshHeaders.push_back(meshHeader);
        }
    }
    
    return true;
}

void BinaryModel::optimizeForVita() {
}

bool BinaryModel::writeHeader(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&data.header), sizeof(BinaryModelHeader));
    return file.good();
}

bool BinaryModel::writeMeshData(std::ofstream& file) {
    for (const auto& meshHeader : data.meshHeaders) {
        file.write(reinterpret_cast<const char*>(&meshHeader), sizeof(BinaryMeshHeader));
    }
    
    for (const auto& vertexData : data.vertexData) {
        file.write(reinterpret_cast<const char*>(vertexData.data()), vertexData.size() * sizeof(float));
    }
    
    for (const auto& indexData : data.indexData) {
        file.write(reinterpret_cast<const char*>(indexData.data()), indexData.size() * sizeof(uint16_t));
    }
    
    return file.good();
}

bool BinaryModel::writeMaterialData(std::ofstream& file) {
    for (const auto& materialHeader : data.materialHeaders) {
        file.write(reinterpret_cast<const char*>(&materialHeader), sizeof(BinaryMaterialHeader));
    }
    return file.good();
}

bool BinaryModel::writeTextureData(std::ofstream& file) {
    for (const auto& textureInfo : data.textureInfos) {
        file.write(reinterpret_cast<const char*>(&textureInfo), sizeof(BinaryTextureInfo));
    }
    
    for (const auto& textureData : data.textureData) {
        file.write(reinterpret_cast<const char*>(textureData.data()), textureData.size());
    }
    
    return file.good();
}

bool BinaryModel::readHeader(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&data.header), sizeof(BinaryModelHeader));
    
    if (!file.good() || !validateHeader(data.header)) {
        std::cerr << "BinaryModel: Invalid binary model header" << std::endl;
        return false;
    }
    
    return true;
}

bool BinaryModel::readMeshData(std::ifstream& file) {
    data.meshHeaders.resize(data.header.meshCount);
    data.vertexData.resize(data.header.meshCount);
    data.indexData.resize(data.header.meshCount);
    
    for (size_t i = 0; i < data.header.meshCount; ++i) {
        file.read(reinterpret_cast<char*>(&data.meshHeaders[i]), sizeof(BinaryMeshHeader));
    }
    
    for (size_t i = 0; i < data.header.meshCount; ++i) {
        const auto& meshHeader = data.meshHeaders[i];
        data.vertexData[i].resize(meshHeader.vertexSize / sizeof(float));
        file.read(reinterpret_cast<char*>(data.vertexData[i].data()), meshHeader.vertexSize);
        
        data.indexData[i].resize(meshHeader.indexSize / sizeof(uint16_t));
        file.read(reinterpret_cast<char*>(data.indexData[i].data()), meshHeader.indexSize);
    }
    
    return file.good();
}

bool BinaryModel::readMaterialData(std::ifstream& file) {
    data.materialHeaders.resize(data.header.materialCount);
    
    for (size_t i = 0; i < data.header.materialCount; ++i) {
        file.read(reinterpret_cast<char*>(&data.materialHeaders[i]), sizeof(BinaryMaterialHeader));
    }
    
    return file.good();
}

bool BinaryModel::readTextureData(std::ifstream& file) {
    return file.good();
}

} // namespace GameEngine
