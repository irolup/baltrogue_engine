#ifndef SCENE_SERIALIZER_H
#define SCENE_SERIALIZER_H

#include <memory>
#include <string>
#include <vector>
#include "Scene/Scene.h"
#include "../../vendor/json/single_include/nlohmann/json.hpp"

namespace GameEngine {

class Material;

class SceneSerializer {
public:
    SceneSerializer() = default;
    ~SceneSerializer() = default;


    static void generateAssetManifests();
    
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

    static std::shared_ptr<SceneNode> duplicateNodeSubtree(std::shared_ptr<SceneNode> node);

    static nlohmann::json serializeNodeToJson(std::shared_ptr<SceneNode> node);
    static std::shared_ptr<SceneNode> deserializeNodeFromJson(const nlohmann::json& nodeJson);

    static void clearMaterialCache();

private:

    static std::shared_ptr<Material> getOrCreateSharedMaterial(const nlohmann::json& materialJson);
    static std::vector<std::shared_ptr<SceneNode>> getAllSceneNodesFromScene(std::shared_ptr<Scene> scene);
    static void collectAllNodesRecursive(std::shared_ptr<SceneNode> node, std::vector<std::shared_ptr<SceneNode>>& allNodes);
    static std::string serializeSceneToJson(std::shared_ptr<Scene> scene);
    static std::shared_ptr<Scene> deserializeSceneFromJson(const nlohmann::json& sceneJson);
    static std::shared_ptr<Scene> deserializeSceneFromJsonText(const std::string& jsonData);
    static std::shared_ptr<Scene> loadSceneFromBytes(const std::vector<uint8_t>& fileBytes, const std::string& sourceLabel);
};

} // namespace GameEngine

#endif // SCENE_SERIALIZER_H
