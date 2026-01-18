#ifndef ANIMATION_COMPONENT_H
#define ANIMATION_COMPONENT_H

#include "Components/Component.h"
#include "Rendering/Skeleton.h"
#include "Rendering/AnimationClip.h"
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace GameEngine {

class AnimationComponent : public Component {
public:
    AnimationComponent();
    virtual ~AnimationComponent();
    
    COMPONENT_TYPE(AnimationComponent)
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void drawInspector() override;
    
    // Skeleton management
    void setSkeleton(const std::string& skeletonName);
    void setSkeleton(std::shared_ptr<Skeleton> skeleton);
    std::shared_ptr<Skeleton> getSkeleton() const { return currentSkeleton; }
    
    // Animation clip management
    void setAnimationClip(const std::string& clipName);
    void setAnimationClip(std::shared_ptr<AnimationClip> clip);
    std::shared_ptr<AnimationClip> getCurrentAnimationClip() const { return currentClip; }
    
    // Animation playback control
    void play();
    void pause();
    void stop();
    void setLoop(bool loop) { isLooping = loop; }
    bool getLoop() const { return isLooping; }
    
    void setSpeed(float speed) { playbackSpeed = speed; }
    float getSpeed() const { return playbackSpeed; }
    
    void setTime(float time);
    float getTime() const { return currentTime; }
    
    bool isPlaying() const { return isPlaying_; }
    
    // Get current bone transforms (for skinning)
    const std::vector<glm::mat4>& getBoneTransforms() const { return boneTransforms; }
    
private:
    std::shared_ptr<Skeleton> currentSkeleton;
    std::shared_ptr<AnimationClip> currentClip;
    
    std::vector<glm::mat4> boneTransforms;  // Final bone transforms for skinning
    std::vector<glm::mat4> localBoneTransforms;  // Local bone transforms
    
    float currentTime;
    float playbackSpeed;
    bool isLooping;
    bool isPlaying_;
    
    void updateBoneTransforms();
    void updateBoneHierarchy(int boneIndex, const glm::mat4& parentTransform);
};

} // namespace GameEngine

#endif // ANIMATION_COMPONENT_H

