#ifndef ANIMATION_MANAGER_H
#define ANIMATION_MANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Rendering/Skeleton.h"
#include "Rendering/AnimationClip.h"

namespace GameEngine {

class AnimationManager {
public:
    static AnimationManager& getInstance();
    
    // Skeleton management
    std::shared_ptr<Skeleton> loadSkeleton(const std::string& filepath, const std::string& skeletonName = "");
    std::shared_ptr<Skeleton> getSkeleton(const std::string& name);
    std::vector<std::string> getAvailableSkeletons() const;
    bool hasSkeleton(const std::string& name) const;
    
    // Animation clip management
    std::shared_ptr<AnimationClip> loadAnimationClip(const std::string& filepath, int animationIndex, const std::string& skeletonName, const std::string& clipName = "");
    std::shared_ptr<AnimationClip> getAnimationClip(const std::string& name);
    std::vector<std::string> getAvailableAnimationClips() const;
    std::vector<std::string> getAnimationClipsForSkeleton(const std::string& skeletonName) const;
    bool hasAnimationClip(const std::string& name) const;
    
    // Asset discovery
    std::vector<std::string> discoverSkeletons(const std::string& directory = "assets/models");
    std::vector<std::string> discoverAnimationClips(const std::string& directory = "assets/models");
    
    // Clear cache
    void clearCache();
    
private:
    AnimationManager() = default;
    ~AnimationManager() = default;
    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<Skeleton>> skeletonCache;
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> animationClipCache;
    std::unordered_map<std::string, std::vector<std::string>> skeletonToAnimations;  // Maps skeleton name to animation clip names
};

} // namespace GameEngine

#endif // ANIMATION_MANAGER_H

