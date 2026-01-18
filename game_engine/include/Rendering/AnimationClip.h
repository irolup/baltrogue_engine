#ifndef ANIMATION_CLIP_H
#define ANIMATION_CLIP_H

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace GameEngine {

enum class InterpolationType {
    LINEAR,
    STEP,
    CUBICSPLINE
};

struct Keyframe {
    float time;
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;
    
    Keyframe() : time(0.0f), translation(0.0f), rotation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f) {}
};

struct BoneAnimation {
    std::string boneName;
    std::vector<Keyframe> keyframes;
    InterpolationType interpolation;
    
    BoneAnimation() : interpolation(InterpolationType::LINEAR) {}
};

class AnimationClip {
public:
    AnimationClip();
    ~AnimationClip();
    
    bool loadFromGLTF(const std::string& filepath, int animationIndex, const std::string& skeletonName);
    
    const std::string& getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    
    const std::string& getSkeletonName() const { return skeletonName; }
    void setSkeletonName(const std::string& skelName) { skeletonName = skelName; }
    
    float getDuration() const { return duration; }
    void setDuration(float d) { duration = d; }
    
    const std::vector<BoneAnimation>& getBoneAnimations() const { return boneAnimations; }
    const BoneAnimation* getBoneAnimation(const std::string& boneName) const;
    
    // Sample animation at a given time (0.0 to duration)
    // Returns transform for a specific bone at the given time
    bool sampleBoneAtTime(const std::string& boneName, float time, glm::mat4& outTransform) const;
    bool sampleBoneAtTime(int boneIndex, float time, glm::mat4& outTransform) const;
    
    // Get all bone transforms at a given time
    void sampleAllBonesAtTime(float time, std::vector<glm::mat4>& outTransforms) const;
    
    const std::string& getFilePath() const { return filePath; }
    void setFilePath(const std::string& path) { filePath = path; }
    
private:
    std::string name;
    std::string skeletonName;  // Name of the skeleton this animation targets
    std::string filePath;
    float duration;
    std::vector<BoneAnimation> boneAnimations;
    std::unordered_map<std::string, int> boneNameToIndex;
    
    void buildNameIndex();
    
    // Interpolation helpers
    glm::vec3 interpolateTranslation(const BoneAnimation& anim, float time) const;
    glm::quat interpolateRotation(const BoneAnimation& anim, float time) const;
    glm::vec3 interpolateScale(const BoneAnimation& anim, float time) const;
    
    // Find keyframe indices for a given time
    void findKeyframeIndices(const std::vector<Keyframe>& keyframes, float time, int& outIndex1, int& outIndex2, float& outT) const;
};

} // namespace GameEngine

#endif // ANIMATION_CLIP_H

