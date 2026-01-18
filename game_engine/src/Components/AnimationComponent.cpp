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
    , isPlaying_(false) {
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
    float oldTime = currentTime;
    currentTime += deltaTime * playbackSpeed;
    
    if (currentTime > currentClip->getDuration()) {
        if (isLooping) {
            currentTime = fmod(currentTime, currentClip->getDuration());
        } else {
            currentTime = currentClip->getDuration();
            isPlaying_ = false;
        }
    }
    
    // Update bone transforms
    updateBoneTransforms();
}

void AnimationComponent::updateBoneTransforms() {
    if (!currentClip || !currentSkeleton) {
        return;
    }
    
    const auto& bones = currentSkeleton->getBones();
    
    std::vector<glm::mat4> sampledTransforms;
    currentClip->sampleAllBonesAtTime(currentTime, sampledTransforms);
    
    for (size_t i = 0; i < bones.size(); ++i) {
        glm::mat4 localTransform = bones[i].bindPose;
        bool usingAnimation = false;
        glm::mat4 sampledTransform;
        
        if (currentClip->sampleBoneAtTime(bones[i].name, currentTime, sampledTransform)) {
            usingAnimation = true;
        } else if (i < sampledTransforms.size() && sampledTransforms[i] != glm::mat4(0.0f)) {
            sampledTransform = sampledTransforms[i];
            usingAnimation = true;
        }
        
        if (usingAnimation) {
            glm::vec3 bindTranslation(bones[i].bindPose[3][0], bones[i].bindPose[3][1], bones[i].bindPose[3][2]);
            glm::vec3 bindScale(
                glm::length(glm::vec3(bones[i].bindPose[0])),
                glm::length(glm::vec3(bones[i].bindPose[1])),
                glm::length(glm::vec3(bones[i].bindPose[2]))
            );
            glm::mat3 bindRotMat = glm::mat3(
                glm::vec3(bones[i].bindPose[0]) / bindScale.x,
                glm::vec3(bones[i].bindPose[1]) / bindScale.y,
                glm::vec3(bones[i].bindPose[2]) / bindScale.z
            );
            glm::quat bindRotation = glm::quat_cast(bindRotMat);
            
            glm::vec3 animTranslation(sampledTransform[3][0], sampledTransform[3][1], sampledTransform[3][2]);
            glm::vec3 animScale(
                glm::length(glm::vec3(sampledTransform[0])),
                glm::length(glm::vec3(sampledTransform[1])),
                glm::length(glm::vec3(sampledTransform[2]))
            );
            glm::mat3 animRotMat = glm::mat3(
                glm::vec3(sampledTransform[0]) / (animScale.x > 0.0001f ? animScale.x : 1.0f),
                glm::vec3(sampledTransform[1]) / (animScale.y > 0.0001f ? animScale.y : 1.0f),
                glm::vec3(sampledTransform[2]) / (animScale.z > 0.0001f ? animScale.z : 1.0f)
            );
            glm::quat animRotation = glm::quat_cast(animRotMat);
            
            glm::vec3 finalTranslation = animTranslation;
            
            bool animRotIsIdentity = (glm::abs(animRotation.w - 1.0f) < 0.0001f && 
                                      glm::abs(animRotation.x) < 0.0001f && 
                                      glm::abs(animRotation.y) < 0.0001f && 
                                      glm::abs(animRotation.z) < 0.0001f);
            bool bindRotIsIdentity = (glm::abs(bindRotation.w - 1.0f) < 0.0001f && 
                                      glm::abs(bindRotation.x) < 0.0001f && 
                                      glm::abs(bindRotation.y) < 0.0001f && 
                                      glm::abs(bindRotation.z) < 0.0001f);
            glm::quat finalRotation = (animRotIsIdentity && !bindRotIsIdentity) ? bindRotation : animRotation;
            
            glm::vec3 finalScale = (glm::length(animScale - glm::vec3(1.0f)) < 0.0001f && 
                                    glm::length(bindScale - glm::vec3(1.0f)) > 0.0001f)
                ? bindScale : animScale;
            
            localTransform = glm::translate(glm::mat4(1.0f), finalTranslation) * 
                            glm::mat4_cast(finalRotation) * 
                            glm::scale(glm::mat4(1.0f), finalScale);
        }
        
        localBoneTransforms[i] = localTransform;
    }
    
    for (size_t i = 0; i < bones.size(); ++i) {
        boneTransforms[i] = glm::mat4(1.0f);
    }
    
    int rootIndex = currentSkeleton->getRootBoneIndex();
    if (rootIndex >= 0) {
        updateBoneHierarchy(rootIndex, glm::mat4(1.0f));
    }
    
    for (size_t i = 0; i < bones.size(); ++i) {
        if (boneTransforms[i] == glm::mat4(1.0f) && bones[i].parentIndex != -1) {
            boneTransforms[i] = localBoneTransforms[i];
        } else if (bones[i].parentIndex == -1 && i != rootIndex) {
            updateBoneHierarchy(static_cast<int>(i), glm::mat4(1.0f));
        }
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
    
    // Debug: Print transform for first few bones (always print, not just every 60 frames)
    if (boneIndex < 3) {
    }
    
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
        
        // Update bone transforms immediately to show first frame
        updateBoneTransforms();
    }
}

void AnimationComponent::play() {
    if (currentClip && currentSkeleton) {
        isPlaying_ = true;
        currentTime = 0.0f;
        
        if (boneTransforms.size() != currentSkeleton->getBoneCount()) {
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

