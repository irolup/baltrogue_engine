#include "Rendering/Skeleton.h"
#include "../../vendor/tinygltf/tiny_gltf.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>

namespace GameEngine {

Skeleton::Skeleton() : name("UnnamedSkeleton") {
}

Skeleton::~Skeleton() {
}

void Skeleton::buildNameIndex() {
    boneNameToIndex.clear();
    for (size_t i = 0; i < bones.size(); ++i) {
        boneNameToIndex[bones[i].name] = static_cast<int>(i);
    }
}

void Skeleton::addBone(const Bone& bone) {
    bones.push_back(bone);
    boneNameToIndex[bone.name] = static_cast<int>(bones.size() - 1);
}

const Bone* Skeleton::getBone(const std::string& name) const {
    auto it = boneNameToIndex.find(name);
    if (it != boneNameToIndex.end()) {
        return &bones[it->second];
    }
    return nullptr;
}

const Bone* Skeleton::getBone(int index) const {
    if (index >= 0 && index < static_cast<int>(bones.size())) {
        return &bones[index];
    }
    return nullptr;
}

int Skeleton::getBoneIndex(const std::string& name) const {
    auto it = boneNameToIndex.find(name);
    if (it != boneNameToIndex.end()) {
        return it->second;
    }
    return -1;
}

int Skeleton::getRootBoneIndex() const {
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].parentIndex == -1) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<int> Skeleton::getChildBones(int boneIndex) const {
    std::vector<int> children;
    if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size())) {
        return children;
    }
    
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].parentIndex == boneIndex) {
            children.push_back(static_cast<int>(i));
        }
    }
    
    return children;
}

bool Skeleton::loadFromGLTF(const std::string& filepath, int skinIndex) {
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    
    bool success = false;
    std::string extension = filepath.substr(filepath.find_last_of('.'));
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".glb") {
        success = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
    } else {
        success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
    }
    
    if (!success) {
        std::cerr << "Skeleton: Failed to load GLTF file: " << err << std::endl;
        return false;
    }
    
    if (skinIndex < 0 || skinIndex >= static_cast<int>(gltfModel.skins.size())) {
        std::cerr << "Skeleton: Invalid skin index: " << skinIndex << std::endl;
        return false;
    }
    
    const auto& skin = gltfModel.skins[skinIndex];
    filePath = filepath;
    
    size_t lastSlash = filepath.find_last_of("/\\");
    size_t lastDot = filepath.find_last_of(".");
    if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
        name = filepath.substr(lastSlash + 1, lastDot - lastSlash - 1);
    } else {
        name = "Skeleton_" + std::to_string(skinIndex);
    }
    
    bones.clear();
    boneNameToIndex.clear();
    
    std::unordered_map<int, int> gltfNodeToBoneIndex;
    
    for (size_t i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(gltfModel.nodes.size())) {
            continue;
        }
        
        const auto& node = gltfModel.nodes[nodeIndex];
        Bone bone;
        bone.name = node.name.empty() ? "Bone_" + std::to_string(i) : node.name;
        bone.gltfNodeIndex = nodeIndex;
        
        if (node.matrix.size() == 16) {
            bone.bindPose = glm::mat4(
                static_cast<float>(node.matrix[0]), static_cast<float>(node.matrix[1]), 
                static_cast<float>(node.matrix[2]), static_cast<float>(node.matrix[3]),
                static_cast<float>(node.matrix[4]), static_cast<float>(node.matrix[5]), 
                static_cast<float>(node.matrix[6]), static_cast<float>(node.matrix[7]),
                static_cast<float>(node.matrix[8]), static_cast<float>(node.matrix[9]), 
                static_cast<float>(node.matrix[10]), static_cast<float>(node.matrix[11]),
                static_cast<float>(node.matrix[12]), static_cast<float>(node.matrix[13]), 
                static_cast<float>(node.matrix[14]), static_cast<float>(node.matrix[15])
            );
        } else {
            glm::vec3 translationVec(0.0f, 0.0f, 0.0f);
            if (node.translation.size() >= 3) {
                translationVec = glm::vec3(
                    static_cast<float>(node.translation[0]), 
                    static_cast<float>(node.translation[1]), 
                    static_cast<float>(node.translation[2])
                );
            }
            glm::mat4 translation = glm::translate(glm::mat4(1.0f), translationVec);
            
            glm::quat rotQuat(1.0f, 0.0f, 0.0f, 0.0f);
            if (node.rotation.size() >= 4) {
                rotQuat = glm::quat(
                    static_cast<float>(node.rotation[3]),
                    static_cast<float>(node.rotation[0]),
                    static_cast<float>(node.rotation[1]),
                    static_cast<float>(node.rotation[2])
                );
            }
            glm::mat4 rotation = glm::mat4_cast(rotQuat);
            
            glm::vec3 scaleVec(1.0f, 1.0f, 1.0f);
            if (node.scale.size() >= 3) {
                scaleVec = glm::vec3(
                    static_cast<float>(node.scale[0]), 
                    static_cast<float>(node.scale[1]), 
                    static_cast<float>(node.scale[2])
                );
            }
            glm::mat4 scale = glm::scale(glm::mat4(1.0f), scaleVec);
            bone.bindPose = translation * rotation * scale;
        }
        
        gltfNodeToBoneIndex[nodeIndex] = static_cast<int>(bones.size());
        bones.push_back(bone);
    }
    
    std::unordered_map<int, int> gltfNodeToBoneMap;
    for (size_t i = 0; i < skin.joints.size(); ++i) {
        gltfNodeToBoneMap[skin.joints[i]] = static_cast<int>(i);
    }
    
    std::unordered_map<int, int> nodeToParent;
    for (size_t i = 0; i < gltfModel.nodes.size(); ++i) {
        const auto& node = gltfModel.nodes[i];
        for (int childNodeIndex : node.children) {
            nodeToParent[childNodeIndex] = static_cast<int>(i);
        }
    }
    
    for (size_t i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];
        bones[i].parentIndex = -1;
        
        int currentParent = nodeIndex;
        while (true) {
            auto it = nodeToParent.find(currentParent);
            if (it == nodeToParent.end()) {
                break;
            }
            int parentNodeIndex = it->second;
            
            auto boneIt = gltfNodeToBoneMap.find(parentNodeIndex);
            if (boneIt != gltfNodeToBoneMap.end()) {
                bones[i].parentIndex = boneIt->second;
                break;
            }
            
            currentParent = parentNodeIndex;
        }
    }
    
    if (skin.inverseBindMatrices >= 0 && skin.inverseBindMatrices < static_cast<int>(gltfModel.accessors.size())) {
        const auto& accessor = gltfModel.accessors[skin.inverseBindMatrices];
        
        if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(gltfModel.bufferViews.size())) {
            std::cerr << "Skeleton: Invalid bufferView index: " << accessor.bufferView << std::endl;
            for (auto& bone : bones) {
                bone.inverseBindPose = glm::inverse(bone.bindPose);
            }
        } else {
            const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
            
            if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(gltfModel.buffers.size())) {
                std::cerr << "Skeleton: Invalid buffer index: " << bufferView.buffer << std::endl;
                for (auto& bone : bones) {
                    bone.inverseBindPose = glm::inverse(bone.bindPose);
                }
            } else {
                const auto& buffer = gltfModel.buffers[bufferView.buffer];
                
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    const float* matrices = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < bones.size() && i < static_cast<size_t>(accessor.count); ++i) {
                        const float* m = &matrices[i * 16];
                        bones[i].inverseBindPose = glm::mat4(
                            m[0], m[1], m[2], m[3],
                            m[4], m[5], m[6], m[7],
                            m[8], m[9], m[10], m[11],
                            m[12], m[13], m[14], m[15]
                        );
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
                    const double* matrices = reinterpret_cast<const double*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < bones.size() && i < static_cast<size_t>(accessor.count); ++i) {
                        const double* m = &matrices[i * 16];
                        bones[i].inverseBindPose = glm::mat4(
                            static_cast<float>(m[0]), static_cast<float>(m[1]), static_cast<float>(m[2]), static_cast<float>(m[3]),
                            static_cast<float>(m[4]), static_cast<float>(m[5]), static_cast<float>(m[6]), static_cast<float>(m[7]),
                            static_cast<float>(m[8]), static_cast<float>(m[9]), static_cast<float>(m[10]), static_cast<float>(m[11]),
                            static_cast<float>(m[12]), static_cast<float>(m[13]), static_cast<float>(m[14]), static_cast<float>(m[15])
                        );
                    }
                }
            }
        }
    } else {
        for (auto& bone : bones) {
            bone.inverseBindPose = glm::inverse(bone.bindPose);
        }
    }
    
    buildNameIndex();
    return true;
}

} // namespace GameEngine

