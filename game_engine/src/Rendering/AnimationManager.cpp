#include "Rendering/AnimationManager.h"
#include "../../vendor/tinygltf/tiny_gltf.h"
#include <iostream>
#include <algorithm>

#ifdef LINUX_BUILD
#include <filesystem>
#endif

namespace GameEngine {

AnimationManager& AnimationManager::getInstance() {
    static AnimationManager instance;
    return instance;
}

std::shared_ptr<Skeleton> AnimationManager::loadSkeleton(const std::string& filepath, const std::string& skeletonName) {
    // Check cache first
    std::string cacheKey = skeletonName.empty() ? filepath : skeletonName;
    auto it = skeletonCache.find(cacheKey);
    if (it != skeletonCache.end()) {
        return it->second;
    }
    
    // Load from GLTF
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
        std::cerr << "AnimationManager: Failed to load GLTF file: " << err << std::endl;
        return nullptr;
    }
    
    // Load all skins as separate skeletons
    for (size_t i = 0; i < gltfModel.skins.size(); ++i) {
        auto skeleton = std::make_shared<Skeleton>();
        if (skeleton->loadFromGLTF(filepath, static_cast<int>(i))) {
            std::string skelName = skeleton->getName();
            if (!skeletonName.empty() && i == 0) {
                skelName = skeletonName;
                skeleton->setName(skelName);
            }
            
            skeletonCache[skelName] = skeleton;
            
            // If this is the requested skeleton, return it
            if (skeletonName.empty() || skelName == skeletonName || i == 0) {
                return skeleton;
            }
        }
    }
    
    return nullptr;
}

std::shared_ptr<Skeleton> AnimationManager::getSkeleton(const std::string& name) {
    auto it = skeletonCache.find(name);
    if (it != skeletonCache.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> AnimationManager::getAvailableSkeletons() const {
    std::vector<std::string> names;
    for (const auto& pair : skeletonCache) {
        names.push_back(pair.first);
    }
    return names;
}

bool AnimationManager::hasSkeleton(const std::string& name) const {
    return skeletonCache.find(name) != skeletonCache.end();
}

std::shared_ptr<AnimationClip> AnimationManager::loadAnimationClip(const std::string& filepath, int animationIndex, const std::string& skeletonName, const std::string& clipName) {
    // Check cache first
    std::string cacheKey = clipName.empty() ? filepath + "_anim_" + std::to_string(animationIndex) : clipName;
    auto it = animationClipCache.find(cacheKey);
    if (it != animationClipCache.end()) {
        return it->second;
    }
    
    // Load from GLTF
    auto clip = std::make_shared<AnimationClip>();
    if (clip->loadFromGLTF(filepath, animationIndex, skeletonName)) {
        if (!clipName.empty()) {
            clip->setName(clipName);
        }
        
        animationClipCache[cacheKey] = clip;
        
        // Link to skeleton
        skeletonToAnimations[skeletonName].push_back(cacheKey);
        
        return clip;
    }
    
    return nullptr;
}

std::shared_ptr<AnimationClip> AnimationManager::getAnimationClip(const std::string& name) {
    auto it = animationClipCache.find(name);
    if (it != animationClipCache.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> AnimationManager::getAvailableAnimationClips() const {
    std::vector<std::string> names;
    for (const auto& pair : animationClipCache) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> AnimationManager::getAnimationClipsForSkeleton(const std::string& skeletonName) const {
    auto it = skeletonToAnimations.find(skeletonName);
    if (it != skeletonToAnimations.end()) {
        return it->second;
    }
    return std::vector<std::string>();
}

bool AnimationManager::hasAnimationClip(const std::string& name) const {
    return animationClipCache.find(name) != animationClipCache.end();
}

std::vector<std::string> AnimationManager::discoverSkeletons(const std::string& directory) {
    std::vector<std::string> skeletons;
    
#ifdef LINUX_BUILD
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".gltf" || extension == ".glb") {
                    // Try to load and extract skeleton names
                    tinygltf::Model gltfModel;
                    tinygltf::TinyGLTF loader;
                    std::string err, warn;
                    
                    bool success = (extension == ".glb") 
                        ? loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath)
                        : loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);
                    
                    if (success && !gltfModel.skins.empty()) {
                        for (size_t i = 0; i < gltfModel.skins.size(); ++i) {
                            std::string skelName = gltfModel.skins[i].name.empty() 
                                ? filePath + "_skin_" + std::to_string(i)
                                : gltfModel.skins[i].name;
                            skeletons.push_back(skelName);
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "AnimationManager: Error discovering skeletons: " << e.what() << std::endl;
    }
#endif
    
    return skeletons;
}

std::vector<std::string> AnimationManager::discoverAnimationClips(const std::string& directory) {
    std::vector<std::string> clips;
    
#ifdef LINUX_BUILD
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".gltf" || extension == ".glb") {
                    // Try to load and extract animation names
                    tinygltf::Model gltfModel;
                    tinygltf::TinyGLTF loader;
                    std::string err, warn;
                    
                    bool success = (extension == ".glb") 
                        ? loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath)
                        : loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);
                    
                    if (success && !gltfModel.animations.empty()) {
                        for (size_t i = 0; i < gltfModel.animations.size(); ++i) {
                            std::string animName = gltfModel.animations[i].name.empty() 
                                ? filePath + "_anim_" + std::to_string(i)
                                : gltfModel.animations[i].name;
                            clips.push_back(animName);
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "AnimationManager: Error discovering animation clips: " << e.what() << std::endl;
    }
#endif
    
    return clips;
}

void AnimationManager::clearCache() {
    skeletonCache.clear();
    animationClipCache.clear();
    skeletonToAnimations.clear();
}

} // namespace GameEngine

