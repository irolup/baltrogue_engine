#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Rendering/Texture.h"

namespace GameEngine {

class TextureManager {
public:
    static TextureManager& getInstance();
    
    std::shared_ptr<Texture> loadTexture(const std::string& filepath);
    std::shared_ptr<Texture> getTexture(const std::string& filepath);
    
    std::vector<std::string> discoverTextures(const std::string& directory);
    std::vector<std::string> discoverAllTextures(const std::string& rootDirectory);
    std::vector<std::string> getAvailableTextures() const;
    
    enum class TextureType {
        DIFFUSE,
        NORMAL,
        ARM,
        SPECULAR,
        EMISSIVE
    };
    
    std::shared_ptr<Texture> getTextureByType(const std::string& basePath, TextureType type);
    
    std::string getTextureTypeSuffix(TextureType type) const;
    bool hasTexture(const std::string& filepath) const;
    void clearCache();
    
private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache;
    std::vector<std::string> discoveredTextures;
    
    bool discoverTexturesInDirectory(const std::string& directory);
    bool discoverTexturesRecursively(const std::string& rootDirectory);
    
#ifndef LINUX_BUILD
    bool readTextureManifest();
    bool writeTextureManifest(const std::vector<std::string>& textures);
#endif
};

} // namespace GameEngine

#endif // TEXTURE_MANAGER_H
