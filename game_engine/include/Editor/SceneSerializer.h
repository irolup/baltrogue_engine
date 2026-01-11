#ifndef SCENE_SERIALIZER_H
#define SCENE_SERIALIZER_H

#include <memory>
#include <string>
#include "Scene/Scene.h"
#include "../../vendor/json/single_include/nlohmann/json.hpp"

namespace GameEngine {

class SceneSerializer {
public:
    SceneSerializer() = default;
    ~SceneSerializer() = default;

    static std::string generateGameMainContent(std::shared_ptr<Scene> scene, const std::vector<std::string>& discoveredTextures);
    static std::string generateVitaMainContent(std::shared_ptr<Scene> scene, const std::vector<std::string>& discoveredTextures);
    static std::string escapeStringForCpp(const std::string& input);
    static std::string generateVisibilityCode(const std::string& nodeName, std::shared_ptr<SceneNode> node);
    
    static void saveSceneToGame(std::shared_ptr<Scene> scene);
    
    static bool saveSceneToFile(std::shared_ptr<Scene> scene, const std::string& filepath);
    static std::shared_ptr<Scene> loadSceneFromFile(const std::string& filepath);
    
    static std::vector<std::string> discoverAndGenerateTextureAssets();
    static void updateMakefileWithTextures(const std::vector<std::string>& discoveredTextures);
    static void generateTextureManifest(const std::vector<std::string>& discoveredTextures);
    
    static std::vector<std::string> discoverAndGenerateFontAssets();
    static void generateFontManifest(const std::vector<std::string>& discoveredFonts);
    
    static std::vector<std::string> discoverAndGenerateModelAssets();
    static void generateModelManifest(const std::vector<std::string>& discoveredModels);
    
    static std::vector<std::string> discoverAndGenerateScriptAssets();
    static void generateScriptManifest(const std::vector<std::string>& discoveredScripts);
    
    static void generateInputMappingAssets();
    static void generateInputMappingManifest();
    
    static void updateMakefileWithAssets(const std::vector<std::string>& discoveredTextures, const std::vector<std::string>& discoveredFonts, const std::vector<std::string>& discoveredModels, const std::vector<std::string>& discoveredScripts);
    
    static std::string convertToVitaPath(const std::string& path);
    static std::string convertToLinuxPath(const std::string& path);

private:
    static std::vector<std::shared_ptr<SceneNode>> getAllSceneNodesFromScene(std::shared_ptr<Scene> scene);
    static void collectAllNodesRecursive(std::shared_ptr<SceneNode> node, std::vector<std::shared_ptr<SceneNode>>& allNodes);
    static std::string sanitizeNodeName(const std::string& name);
    
    static nlohmann::json serializeNodeToJson(std::shared_ptr<SceneNode> node);
    static std::shared_ptr<SceneNode> deserializeNodeFromJson(const nlohmann::json& nodeJson);
    static std::string serializeSceneToJson(std::shared_ptr<Scene> scene);
    static std::shared_ptr<Scene> deserializeSceneFromJson(const std::string& jsonData);
};

} // namespace GameEngine

#endif // SCENE_SERIALIZER_H
