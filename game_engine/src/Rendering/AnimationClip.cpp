#include "Rendering/AnimationClip.h"
#include "../../vendor/tinygltf/tiny_gltf.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace GameEngine {

AnimationClip::AnimationClip() 
    : name("UnnamedAnimation")
    , duration(0.0f) {
}

AnimationClip::~AnimationClip() {
}

void AnimationClip::buildNameIndex() {
    boneNameToIndex.clear();
    for (size_t i = 0; i < boneAnimations.size(); ++i) {
        boneNameToIndex[boneAnimations[i].boneName] = static_cast<int>(i);
    }
}

const BoneAnimation* AnimationClip::getBoneAnimation(const std::string& boneName) const {
    auto it = boneNameToIndex.find(boneName);
    if (it != boneNameToIndex.end()) {
        return &boneAnimations[it->second];
    }
    return nullptr;
}

void AnimationClip::findKeyframeIndices(const std::vector<Keyframe>& keyframes, float time, int& outIndex1, int& outIndex2, float& outT) const {
    if (keyframes.empty()) {
        outIndex1 = outIndex2 = 0;
        outT = 0.0f;
        return;
    }
    
    if (time <= keyframes[0].time) {
        outIndex1 = outIndex2 = 0;
        outT = 0.0f;
        return;
    }
    
    if (time >= keyframes[keyframes.size() - 1].time) {
        outIndex1 = outIndex2 = static_cast<int>(keyframes.size() - 1);
        outT = 0.0f;
        return;
    }
    
    // Binary search for the two keyframes
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        if (time >= keyframes[i].time && time <= keyframes[i + 1].time) {
            outIndex1 = static_cast<int>(i);
            outIndex2 = static_cast<int>(i + 1);
            float deltaTime = keyframes[i + 1].time - keyframes[i].time;
            if (deltaTime > 0.0001f) {
                outT = (time - keyframes[i].time) / deltaTime;
            } else {
                outT = 0.0f;
            }
            return;
        }
    }
    
    outIndex1 = outIndex2 = 0;
    outT = 0.0f;
}

glm::vec3 AnimationClip::interpolateTranslation(const BoneAnimation& anim, float time) const {
    if (anim.keyframes.empty()) {
        return glm::vec3(0.0f);
    }
    
    int idx1, idx2;
    float t;
    findKeyframeIndices(anim.keyframes, time, idx1, idx2, t);
    
    if (idx1 == idx2) {
        return anim.keyframes[idx1].translation;
    }
    
    const auto& kf1 = anim.keyframes[idx1];
    const auto& kf2 = anim.keyframes[idx2];
    
    if (anim.interpolation == InterpolationType::STEP) {
        return kf1.translation;
    } else if (anim.interpolation == InterpolationType::LINEAR) {
        return glm::mix(kf1.translation, kf2.translation, t);
    } else {
        return glm::mix(kf1.translation, kf2.translation, t);
    }
}

glm::quat AnimationClip::interpolateRotation(const BoneAnimation& anim, float time) const {
    if (anim.keyframes.empty()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    
    int idx1, idx2;
    float t;
    findKeyframeIndices(anim.keyframes, time, idx1, idx2, t);
    
    if (idx1 == idx2) {
        return anim.keyframes[idx1].rotation;
    }
    
    const auto& kf1 = anim.keyframes[idx1];
    const auto& kf2 = anim.keyframes[idx2];
    
    if (anim.interpolation == InterpolationType::STEP) {
        return kf1.rotation;
    } else if (anim.interpolation == InterpolationType::LINEAR) {
        return glm::slerp(kf1.rotation, kf2.rotation, t);
    } else {
        // CUBICSPLINE - simplified slerp for now
        return glm::slerp(kf1.rotation, kf2.rotation, t);
    }
}

glm::vec3 AnimationClip::interpolateScale(const BoneAnimation& anim, float time) const {
    if (anim.keyframes.empty()) {
        return glm::vec3(1.0f);
    }
    
    int idx1, idx2;
    float t;
    findKeyframeIndices(anim.keyframes, time, idx1, idx2, t);
    
    if (idx1 == idx2) {
        return anim.keyframes[idx1].scale;
    }
    
    const auto& kf1 = anim.keyframes[idx1];
    const auto& kf2 = anim.keyframes[idx2];
    
    if (anim.interpolation == InterpolationType::STEP) {
        return kf1.scale;
    } else if (anim.interpolation == InterpolationType::LINEAR) {
        return glm::mix(kf1.scale, kf2.scale, t);
    } else {
        // CUBICSPLINE - simplified linear for now
        return glm::mix(kf1.scale, kf2.scale, t);
    }
}

bool AnimationClip::sampleBoneAtTime(const std::string& boneName, float time, glm::mat4& outTransform) const {
    const BoneAnimation* anim = getBoneAnimation(boneName);
    
    if (!anim || anim->keyframes.empty()) {
        return false;
    }
    
    glm::vec3 translation = interpolateTranslation(*anim, time);
    glm::quat rotation = interpolateRotation(*anim, time);
    glm::vec3 scale = interpolateScale(*anim, time);
    
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    
    outTransform = T * R * S;
    return true;
}

bool AnimationClip::sampleBoneAtTime(int boneIndex, float time, glm::mat4& outTransform) const {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(boneAnimations.size())) {
        return false;
    }
    
    return sampleBoneAtTime(boneAnimations[boneIndex].boneName, time, outTransform);
}

void AnimationClip::sampleAllBonesAtTime(float time, std::vector<glm::mat4>& outTransforms) const {
    outTransforms.clear();
    outTransforms.resize(boneAnimations.size());
    
    for (size_t i = 0; i < boneAnimations.size(); ++i) {
        sampleBoneAtTime(boneAnimations[i].boneName, time, outTransforms[i]);
    }
}

bool AnimationClip::loadFromGLTF(const std::string& filepath, int animationIndex, const std::string& skelName) {
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
        std::cerr << "AnimationClip: Failed to load GLTF file: " << err << std::endl;
        return false;
    }
    
    if (animationIndex < 0 || animationIndex >= static_cast<int>(gltfModel.animations.size())) {
        std::cerr << "AnimationClip: Invalid animation index: " << animationIndex << std::endl;
        return false;
    }
    
    const auto& gltfAnim = gltfModel.animations[animationIndex];
    filePath = filepath;
    skeletonName = skelName;
    
    // Extract animation name
    name = gltfAnim.name.empty() ? "Animation_" + std::to_string(animationIndex) : gltfAnim.name;
    
    boneAnimations.clear();
    boneNameToIndex.clear();
    duration = 0.0f;
    
    // Process each channel
    std::unordered_map<int, BoneAnimation*> nodeToBoneAnim;  // Maps glTF node index to BoneAnimation
    
    for (const auto& channel : gltfAnim.channels) {
        if (channel.target_node < 0 || channel.target_node >= static_cast<int>(gltfModel.nodes.size())) {
            continue;
        }
        
        const auto& node = gltfModel.nodes[channel.target_node];
        std::string boneName = node.name.empty() ? "Node_" + std::to_string(channel.target_node) : node.name;
        
        BoneAnimation* boneAnim = nullptr;
        auto it = nodeToBoneAnim.find(channel.target_node);
        if (it == nodeToBoneAnim.end()) {
            BoneAnimation newAnim;
            newAnim.boneName = boneName;
            boneAnimations.push_back(newAnim);
            boneAnim = &boneAnimations.back();
            nodeToBoneAnim[channel.target_node] = boneAnim;
        } else {
            boneAnim = it->second;
        }
        
        if (channel.sampler < 0 || channel.sampler >= static_cast<int>(gltfAnim.samplers.size())) {
            continue;
        }
        
        const auto& sampler = gltfAnim.samplers[channel.sampler];
        
        if (channel.target_path == "translation") {
            boneAnim->interpolation = InterpolationType::LINEAR;
        } else {
            if (boneAnim->interpolation != InterpolationType::LINEAR) {
                if (sampler.interpolation == "LINEAR") {
                    boneAnim->interpolation = InterpolationType::LINEAR;
                } else if (sampler.interpolation == "STEP") {
                    boneAnim->interpolation = InterpolationType::STEP;
                } else if (sampler.interpolation == "CUBICSPLINE") {
                    boneAnim->interpolation = InterpolationType::CUBICSPLINE;
                } else {
                    boneAnim->interpolation = InterpolationType::LINEAR;
                }
            }
        }
        
        if (sampler.input < 0 || sampler.input >= static_cast<int>(gltfModel.accessors.size())) {
            continue;
        }
        
        const auto& inputAccessor = gltfModel.accessors[sampler.input];
        const auto& inputBufferView = gltfModel.bufferViews[inputAccessor.bufferView];
        const auto& inputBuffer = gltfModel.buffers[inputBufferView.buffer];
        const float* times = reinterpret_cast<const float*>(&inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset]);
        
        if (sampler.output < 0 || sampler.output >= static_cast<int>(gltfModel.accessors.size())) {
            continue;
        }
        
        const auto& outputAccessor = gltfModel.accessors[sampler.output];
        const auto& outputBufferView = gltfModel.bufferViews[outputAccessor.bufferView];
        const auto& outputBuffer = gltfModel.buffers[outputBufferView.buffer];
        const float* values = reinterpret_cast<const float*>(&outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset]);
        
        // Update duration
        int keyframeCount = inputAccessor.count;
        if (keyframeCount > 0) {
            float lastTime = times[keyframeCount - 1];
            if (lastTime > duration) {
                duration = lastTime;
            }
        }
        
        // Create keyframes
        int componentCount = 0;
        if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
            componentCount = 3;
        } else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
            componentCount = 4;
        } else if (outputAccessor.type == TINYGLTF_TYPE_SCALAR) {
            componentCount = 1;
        }
        
        // Ensure we have enough keyframes
        if (boneAnim->keyframes.size() < static_cast<size_t>(keyframeCount)) {
            boneAnim->keyframes.resize(keyframeCount);
        }
        
        bool hasTranslation = false, hasRotation = false, hasScale = false;
        for (int i = 0; i < keyframeCount; ++i) {
            boneAnim->keyframes[i].time = times[i];
            
            if (channel.target_path == "translation") {
                hasTranslation = true;
                boneAnim->keyframes[i].translation = glm::vec3(
                    values[i * componentCount + 0],
                    values[i * componentCount + 1],
                    values[i * componentCount + 2]
                );
            } else if (channel.target_path == "rotation") {
                hasRotation = true;
                // glTF stores quaternions as (x, y, z, w)
                boneAnim->keyframes[i].rotation = glm::quat(
                    values[i * componentCount + 3],  // w
                    values[i * componentCount + 0],  // x
                    values[i * componentCount + 1],  // y
                    values[i * componentCount + 2]   // z
                );
            } else if (channel.target_path == "scale") {
                hasScale = true;
                boneAnim->keyframes[i].scale = glm::vec3(
                    values[i * componentCount + 0],
                    values[i * componentCount + 1],
                    values[i * componentCount + 2]
                );
            }
        }
    }
    
    buildNameIndex();
    
    return true;
}

} // namespace GameEngine

