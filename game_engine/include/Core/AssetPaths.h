#ifndef ASSET_PATHS_H
#define ASSET_PATHS_H

#include <string>

namespace GameEngine {

// Paths carried by scenes, components and Lua stay in their portable form
// ("assets/textures/...") and are resolved only when a file is opened, so
// the same scene loads on PC and Vita. Nothing needs to be re-serialized.
class AssetPaths {
public:
    static void setProjectRoot(const std::string& root);
    static const std::string& getProjectRoot();

    static void setAssetRoot(const std::string& root);
    static const std::string& getAssetRoot();

    // Project-relative ("assets/scenes/....json", "config/input_mappings.txt")
    // or asset-relative ("scenes/main.json") in, openable path out. Paths that
    // are already absolute or device-prefixed come back untouched.
    static std::string resolve(const std::string& path);

    // Always asset-relative: resolveAsset("shaders/lighting.vert").
    static std::string resolveAsset(const std::string& path);

    static std::string resolveTexture(const std::string& path);

    // Portable form to store in scenes, templates and settings
    static std::string toPortable(const std::string& path);

    static bool hasDevicePrefix(const std::string& path);
    static bool isAbsolute(const std::string& path);
    static bool exists(const std::string& path);

private:
    static std::string normalize(const std::string& path);
    static std::string join(const std::string& base, const std::string& path);
    static std::string firstSegment(const std::string& path);
    static std::string dropFirstSegment(const std::string& path);
    static bool isAssetDirectory(const std::string& segment);
    static std::string resolveProjectRelative(const std::string& path);

    static std::string projectRoot;
    static std::string assetRoot;
    static std::string assetPrefix;
};

}

#endif // ASSET_PATHS_H
