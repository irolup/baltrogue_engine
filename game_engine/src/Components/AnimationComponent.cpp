#include "Components/AnimationComponent.h"
#include "Components/ModelRenderer.h"
#include "Rendering/AnimationManager.h"
#include "Scene/SceneNode.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <functional>

#ifdef EDITOR_BUILD
#include "imgui.h"
#endif

namespace GameEngine {

AnimationComponent::AnimationComponent()
    : currentTime(0.0f)
    , playbackSpeed(1.0f)
    , isLooping(true)
    , isPlaying_(false)
    , enableRootMotion(false) {
}

AnimationComponent::~AnimationComponent() {
}

void AnimationComponent::start() {
    if (!currentSkeleton && owner) {
        auto modelRenderer = owner->getComponent<ModelRenderer>();
        if (modelRenderer && modelRenderer->isModelLoaded()) {
            std::string modelName = modelRenderer->getModelName();
            auto& animManager = AnimationManager::getInstance();
            
            std::string skeletonName = modelName + "_Skeleton_0";
            auto skeleton = animManager.getSkeleton(skeletonName);
            
            if (!skeleton) {
                auto availableSkeletons = animManager.getAvailableSkeletons();
                if (!availableSkeletons.empty()) {
                    skeleton = animManager.getSkeleton(availableSkeletons[0]);
                }
            }
            
            if (skeleton) {
                setSkeleton(skeleton);
            }
        }
    }
    
    if (currentSkeleton && !currentClip && owner) {
        auto modelRenderer = owner->getComponent<ModelRenderer>();
        if (modelRenderer && modelRenderer->isModelLoaded()) {
            std::string modelName = modelRenderer->getModelName();
            auto& animManager = AnimationManager::getInstance();
            
            std::string skeletonName = currentSkeleton->getName();
            auto clips = animManager.getAnimationClipsForSkeleton(skeletonName);
            
            if (clips.empty()) {
                std::string clipName = modelName + "_Animation_0";
                auto clip = animManager.getAnimationClip(clipName);
                if (clip) {
                    clips.push_back(clipName);
                }
            }
            
            if (clips.empty()) {
                auto allClips = animManager.getAvailableAnimationClips();
                if (!allClips.empty()) {
                    clips.push_back(allClips[0]);
                }
            }
            
            if (!clips.empty()) {
                auto clip = animManager.getAnimationClip(clips[0]);
                if (clip) {
                    setAnimationClip(clip);
                }
            }
        }
    }
    
    if (currentClip && currentSkeleton) {
        boneTransforms.resize(currentSkeleton->getBoneCount());
        localBoneTransforms.resize(currentSkeleton->getBoneCount());
        std::fill(boneTransforms.begin(), boneTransforms.end(), glm::mat4(1.0f));
        std::fill(localBoneTransforms.begin(), localBoneTransforms.end(), glm::mat4(1.0f));
    }
}

void AnimationComponent::update(float deltaTime) {
    static int updateCount = 0;
    updateCount++;
    
    if (!currentSkeleton && owner) {
        auto modelRenderer = owner->getComponent<ModelRenderer>();
        if (modelRenderer && modelRenderer->isModelLoaded()) {
            std::string modelName = modelRenderer->getModelName();
            auto& animManager = AnimationManager::getInstance();
            
            std::string skeletonName = modelName + "_Skeleton_0";
            auto skeleton = animManager.getSkeleton(skeletonName);
            
            if (!skeleton) {
                auto availableSkeletons = animManager.getAvailableSkeletons();
                if (!availableSkeletons.empty()) {
                    skeleton = animManager.getSkeleton(availableSkeletons[0]);
                }
            }
            
            if (skeleton) {
                setSkeleton(skeleton);
            }
        }
    }
    
    if (currentSkeleton && !currentClip && owner) {
        auto modelRenderer = owner->getComponent<ModelRenderer>();
        if (modelRenderer && modelRenderer->isModelLoaded()) {
            auto& animManager = AnimationManager::getInstance();
            
            std::string skeletonName = currentSkeleton->getName();
            auto clips = animManager.getAnimationClipsForSkeleton(skeletonName);
            
            if (clips.empty() && modelRenderer) {
                std::string modelName = modelRenderer->getModelName();
                std::string clipName = modelName + "_Animation_0";
                auto clip = animManager.getAnimationClip(clipName);
                if (clip) {
                    clips.push_back(clipName);
                }
            }
            
            if (clips.empty()) {
                auto allClips = animManager.getAvailableAnimationClips();
                if (!allClips.empty()) {
                    clips.push_back(allClips[0]);
                }
            }
            
            if (!clips.empty()) {
                auto clip = animManager.getAnimationClip(clips[0]);
                if (clip) {
                    setAnimationClip(clip);
                }
            }
        }
    }
    
    if (!isPlaying_) {
        if (currentClip && currentSkeleton && boneTransforms.empty()) {
            boneTransforms.resize(currentSkeleton->getBoneCount());
            localBoneTransforms.resize(currentSkeleton->getBoneCount());
            std::fill(boneTransforms.begin(), boneTransforms.end(), glm::mat4(1.0f));
            std::fill(localBoneTransforms.begin(), localBoneTransforms.end(), glm::mat4(1.0f));
            updateBoneTransforms();
        }
        
        return;
    }
    
    if (!currentClip || !currentSkeleton) {
        return;
    }
    
    // Update animation time
    currentTime += deltaTime * playbackSpeed;
    
    if (currentTime > currentClip->getDuration()) {
        if (isLooping) {
            currentTime = fmod(currentTime, currentClip->getDuration());
        } else {
            currentTime = currentClip->getDuration();
            isPlaying_ = false;
        }
    }
    
    updateBoneTransforms();
}

void AnimationComponent::updateBoneTransforms() {
    if (!currentClip || !currentSkeleton) return;

    const auto& bones = currentSkeleton->getBones();

    std::vector<glm::mat4> sampledTransforms;
    currentClip->sampleAllBonesAtTime(currentTime, sampledTransforms);

    int rootIndex = currentSkeleton->getRootBoneIndex();

    for (size_t i = 0; i < bones.size(); ++i) {
        glm::mat4 localTransform = bones[i].bindPose;

        if (i < sampledTransforms.size() && sampledTransforms[i] != glm::mat4(0.0f)) {
            const BoneAnimation* anim = currentClip->getBoneAnimation(bones[i].name);
            
            if (anim && (!anim->translations.empty() || !anim->rotations.empty() || !anim->scales.empty())) {
                bool hasAllChannels = !anim->translations.empty() && !anim->rotations.empty() && !anim->scales.empty();
                
                if (hasAllChannels) {
                    localTransform = sampledTransforms[i];
                } else {
                    // Extract bind pose TRS manually (more reliable than decomposing)
                    glm::vec3 bindTranslation = glm::vec3(bones[i].bindPose[3]);
                    glm::mat3 bindRotScale = glm::mat3(bones[i].bindPose);
                    glm::vec3 bindScale(
                        glm::length(bindRotScale[0]),
                        glm::length(bindRotScale[1]),
                        glm::length(bindRotScale[2])
                    );
                    glm::mat3 bindRotMat = glm::mat3(
                        bindRotScale[0] / (bindScale.x > 0.0001f ? bindScale.x : 1.0f),
                        bindRotScale[1] / (bindScale.y > 0.0001f ? bindScale.y : 1.0f),
                        bindRotScale[2] / (bindScale.z > 0.0001f ? bindScale.z : 1.0f)
                    );
                    glm::quat bindRotation = glm::quat_cast(bindRotMat);
                    
                    // Sample each channel individually, using bind pose as default for missing channels
                    glm::vec3 finalTranslation = bindTranslation;
                    glm::quat finalRotation = bindRotation;
                    glm::vec3 finalScale = bindScale;
                    
                    // Helper function to find keyframe indices for Vec3Key (C++11 compatible)
                    auto findVec3Indices = [](const std::vector<Vec3Key>& keys, float time, int& idx1, int& idx2, float& t) {
                        if (keys.empty()) {
                            idx1 = idx2 = 0;
                            t = 0.0f;
                            return;
                        }
                        if (time <= keys[0].time) {
                            idx1 = idx2 = 0;
                            t = 0.0f;
                            return;
                        }
                        if (time >= keys.back().time) {
                            idx1 = idx2 = static_cast<int>(keys.size() - 1);
                            t = 0.0f;
                            return;
                        }
                        idx1 = 0;
                        for (size_t i = 0; i < keys.size() - 1; ++i) {
                            if (time >= keys[i].time && time <= keys[i + 1].time) {
                                idx1 = static_cast<int>(i);
                                idx2 = static_cast<int>(i + 1);
                                float dt = keys[idx2].time - keys[idx1].time;
                                t = (dt > 0.0001f) ? (time - keys[idx1].time) / dt : 0.0f;
                                return;
                            }
                        }
                        idx1 = idx2 = static_cast<int>(keys.size() - 1);
                        t = 0.0f;
                    };
                    
                    // Helper function to find keyframe indices for QuatKey (C++11 compatible)
                    auto findQuatIndices = [](const std::vector<QuatKey>& keys, float time, int& idx1, int& idx2, float& t) {
                        if (keys.empty()) {
                            idx1 = idx2 = 0;
                            t = 0.0f;
                            return;
                        }
                        if (time <= keys[0].time) {
                            idx1 = idx2 = 0;
                            t = 0.0f;
                            return;
                        }
                        if (time >= keys.back().time) {
                            idx1 = idx2 = static_cast<int>(keys.size() - 1);
                            t = 0.0f;
                            return;
                        }
                        idx1 = 0;
                        for (size_t i = 0; i < keys.size() - 1; ++i) {
                            if (time >= keys[i].time && time <= keys[i + 1].time) {
                                idx1 = static_cast<int>(i);
                                idx2 = static_cast<int>(i + 1);
                                float dt = keys[idx2].time - keys[idx1].time;
                                t = (dt > 0.0001f) ? (time - keys[idx1].time) / dt : 0.0f;
                                return;
                            }
                        }
                        idx1 = idx2 = static_cast<int>(keys.size() - 1);
                        t = 0.0f;
                    };
                    
                    if (!anim->translations.empty()) {
                        int idx1, idx2;
                        float t;
                        findVec3Indices(anim->translations, currentTime, idx1, idx2, t);
                        if (idx1 == idx2) {
                            finalTranslation = anim->translations[idx1].value;
                        } else {
                            const auto& kf1 = anim->translations[idx1];
                            const auto& kf2 = anim->translations[idx2];
                            if (anim->interpolation == InterpolationType::STEP) {
                                finalTranslation = kf1.value;
                            } else {
                                finalTranslation = glm::mix(kf1.value, kf2.value, t);
                            }
                        }
                    }
                    
                    if (!anim->rotations.empty()) {
                        int idx1, idx2;
                        float t;
                        findQuatIndices(anim->rotations, currentTime, idx1, idx2, t);
                        if (idx1 == idx2) {
                            finalRotation = anim->rotations[idx1].value;
                        } else {
                            const auto& kf1 = anim->rotations[idx1];
                            const auto& kf2 = anim->rotations[idx2];
                            if (anim->interpolation == InterpolationType::STEP) {
                                finalRotation = kf1.value;
                            } else {
                                finalRotation = glm::slerp(kf1.value, kf2.value, t);
                            }
                        }
                    }
                    
                    if (!anim->scales.empty()) {
                        int idx1, idx2;
                        float t;
                        findVec3Indices(anim->scales, currentTime, idx1, idx2, t);
                        if (idx1 == idx2) {
                            finalScale = anim->scales[idx1].value;
                        } else {
                            const auto& kf1 = anim->scales[idx1];
                            const auto& kf2 = anim->scales[idx2];
                            if (anim->interpolation == InterpolationType::STEP) {
                                finalScale = kf1.value;
                            } else {
                                finalScale = glm::mix(kf1.value, kf2.value, t);
                            }
                        }
                    }
                    
                    localTransform = glm::translate(glm::mat4(1.0f), finalTranslation) * 
                                    glm::mat4_cast(finalRotation) * 
                                    glm::scale(glm::mat4(1.0f), finalScale);
                }
            } else {
                localTransform = sampledTransforms[i];
            }
        }
        if ((int)i == rootIndex && !enableRootMotion) {
            localTransform[3][0] = 0.0f;
            localTransform[3][1] = 0.0f;
            localTransform[3][2] = 0.0f;
        }

        localBoneTransforms[i] = localTransform;
    }

    if (rootIndex >= 0) {
        updateBoneHierarchy(rootIndex, glm::mat4(1.0f));
    }

    for (size_t i = 0; i < bones.size(); ++i) {
        boneTransforms[i] = boneTransforms[i] * bones[i].inverseBindPose;
    }
}

void AnimationComponent::updateBoneHierarchy(int boneIndex, const glm::mat4& parentTransform) {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(localBoneTransforms.size())) {
        return;
    }
    
    const auto& bones = currentSkeleton->getBones();
    if (boneIndex >= static_cast<int>(bones.size())) {
        return;
    }
    
    boneTransforms[boneIndex] = parentTransform * localBoneTransforms[boneIndex];
    
    auto children = currentSkeleton->getChildBones(boneIndex);
    for (int childIndex : children) {
        if (childIndex != boneIndex && childIndex >= 0 && childIndex < static_cast<int>(bones.size())) {
            updateBoneHierarchy(childIndex, boneTransforms[boneIndex]);
        }
    }
}

void AnimationComponent::setSkeleton(const std::string& skeletonName) {
    auto& animManager = AnimationManager::getInstance();
    auto skeleton = animManager.getSkeleton(skeletonName);
    if (!skeleton) {
        std::cerr << "AnimationComponent: Skeleton not found: " << skeletonName << std::endl;
        return;
    }
    
    setSkeleton(skeleton);
}

void AnimationComponent::setSkeleton(std::shared_ptr<Skeleton> skeleton) {
    currentSkeleton = skeleton;
    
    if (currentSkeleton) {
        boneTransforms.resize(currentSkeleton->getBoneCount());
        localBoneTransforms.resize(currentSkeleton->getBoneCount());
        std::fill(boneTransforms.begin(), boneTransforms.end(), glm::mat4(1.0f));
        std::fill(localBoneTransforms.begin(), localBoneTransforms.end(), glm::mat4(1.0f));
    }
}

void AnimationComponent::setAnimationClip(const std::string& clipName) {
    auto& animManager = AnimationManager::getInstance();
    auto clip = animManager.getAnimationClip(clipName);
    if (!clip) {
        std::cerr << "AnimationComponent: Animation clip not found: " << clipName << std::endl;
        return;
    }
    
    setAnimationClip(clip);
}

void AnimationComponent::setAnimationClip(std::shared_ptr<AnimationClip> clip) {
    currentClip = clip;
    currentTime = 0.0f;
    
    if (currentClip && currentSkeleton) {
        boneTransforms.resize(currentSkeleton->getBoneCount());
        localBoneTransforms.resize(currentSkeleton->getBoneCount());
        std::fill(boneTransforms.begin(), boneTransforms.end(), glm::mat4(1.0f));
        std::fill(localBoneTransforms.begin(), localBoneTransforms.end(), glm::mat4(1.0f));
        
        updateBoneTransforms();
    }
}

void AnimationComponent::play() {
    if (currentClip && currentSkeleton) {
        isPlaying_ = true;
        currentTime = 0.0f;
        
        if (boneTransforms.size() != static_cast<size_t>(currentSkeleton->getBoneCount())) {
            boneTransforms.resize(currentSkeleton->getBoneCount());
            localBoneTransforms.resize(currentSkeleton->getBoneCount());
            std::fill(boneTransforms.begin(), boneTransforms.end(), glm::mat4(1.0f));
            std::fill(localBoneTransforms.begin(), localBoneTransforms.end(), glm::mat4(1.0f));
        }
        
        updateBoneTransforms();
    }
}

void AnimationComponent::pause() {
    isPlaying_ = false;
}

void AnimationComponent::stop() {
    isPlaying_ = false;
    currentTime = 0.0f;
}

void AnimationComponent::setTime(float time) {
    if (currentClip) {
        currentTime = glm::clamp(time, 0.0f, currentClip->getDuration());
        updateBoneTransforms();
    }
}

void AnimationComponent::drawInspector() {
#ifdef EDITOR_BUILD
    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.8f, 1.0f), "🎬 Animation Component");
    
    // Skeleton info
    if (currentSkeleton) {
        ImGui::Text("Skeleton: %s", currentSkeleton->getName().c_str());
        ImGui::Text("Bones: %d", currentSkeleton->getBoneCount());
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "No skeleton set");
    }
    
    // Animation clip info
    if (currentClip) {
        ImGui::Text("Animation: %s", currentClip->getName().c_str());
        ImGui::Text("Duration: %.2fs", currentClip->getDuration());
        ImGui::Text("Bone Animations: %zu", currentClip->getBoneAnimations().size());
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "No animation clip set");
    }
    
    // Playback state
    ImGui::Separator();
    ImGui::Text("Playing: %s", isPlaying_ ? "Yes" : "No");
    ImGui::Text("Current Time: %.2fs", currentTime);
    if (currentClip) {
        ImGui::Text("Progress: %.1f%%", (currentTime / currentClip->getDuration()) * 100.0f);
    }
    
    // Playback controls
    ImGui::Separator();
    if (ImGui::Button(isPlaying_ ? "Pause" : "Play")) {
        if (isPlaying_) {
            pause();
        } else {
            play();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        stop();
    }
    
    // Playback settings
    ImGui::Separator();
    bool looping = isLooping;
    if (ImGui::Checkbox("Loop", &looping)) {
        setLoop(looping);
    }
    
    bool rootMotion = enableRootMotion;
    if (ImGui::Checkbox("Enable Root Motion", &rootMotion)) {
        setRootMotionEnabled(rootMotion);
    }
    
    float speed = playbackSpeed;
    if (ImGui::SliderFloat("Speed", &speed, 0.0f, 2.0f)) {
        setSpeed(speed);
    }
    
    // Bone transforms info
    ImGui::Separator();
    ImGui::Text("Bone Transforms: %zu", boneTransforms.size());
#endif
}

} // namespace GameEngine

