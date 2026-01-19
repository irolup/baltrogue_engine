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

struct Vec3Key {
    float time;
    glm::vec3 value;
    
    Vec3Key() : time(0.0f), value(0.0f) {}
    Vec3Key(float t, const glm::vec3& v) : time(t), value(v) {}
};

struct QuatKey {
    float time;
    glm::quat value;
    
    QuatKey() : time(0.0f), value(1.0f, 0.0f, 0.0f, 0.0f) {}
    QuatKey(float t, const glm::quat& v) : time(t), value(v) {}
};

struct BoneAnimation {
    std::string boneName;
    std::vector<Vec3Key> translations;
    std::vector<QuatKey> rotations;
    std::vector<Vec3Key> scales;
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
    
    // Find keyframe indices for a given time (template for Vec3Key and QuatKey)
    template<typename KeyType>
    void findKeyframeIndices(const std::vector<KeyType>& keyframes, float time, int& outIndex1, int& outIndex2, float& outT) const {
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
    
    // Sample helpers for separate keyframe types
    glm::vec3 sampleVec3(const std::vector<Vec3Key>& keys, float time, const glm::vec3& defaultValue, InterpolationType interpolation) const;
    glm::quat sampleQuat(const std::vector<QuatKey>& keys, float time, const glm::quat& defaultValue, InterpolationType interpolation) const;
};

} // namespace GameEngine

#endif // ANIMATION_CLIP_H

