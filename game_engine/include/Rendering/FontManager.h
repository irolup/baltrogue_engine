#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

namespace GameEngine {
    class Texture;
}

#include "../../vendor/stb/stb_truetype.h"

namespace GameEngine {

struct FontAtlas {
    std::shared_ptr<Texture> texture;
    std::vector<stbtt_packedchar> packedChars;
    std::vector<stbtt_aligned_quad> alignedQuads;
    uint32_t atlasWidth;
    uint32_t atlasHeight;
    uint32_t fontSize;
    uint32_t charsToInclude;
    uint32_t firstCharCodePoint;
    
    FontAtlas() : atlasWidth(0), atlasHeight(0), fontSize(0), charsToInclude(0), firstCharCodePoint(0) {}
};

class FontManager {
public:
    static FontManager& getInstance();
    
    std::shared_ptr<FontAtlas> loadFont(const std::string& fontPath, float fontSize, 
                                       uint32_t atlasWidth = 512, uint32_t atlasHeight = 512,
                                       uint32_t charsToInclude = 95, uint32_t firstCharCodePoint = 32);
    
    std::shared_ptr<FontAtlas> getFont(const std::string& fontPath, float fontSize);
    
    bool generateFontAtlas(const std::string& fontPath, float fontSize, FontAtlas& atlas,
                          uint32_t atlasWidth = 512, uint32_t atlasHeight = 512,
                          uint32_t charsToInclude = 95, uint32_t firstCharCodePoint = 32);
    
    void clearCache();
    void removeFont(const std::string& fontPath, float fontSize);
    
    std::string getFontKey(const std::string& fontPath, float fontSize) const;
    bool isFontLoaded(const std::string& fontPath, float fontSize) const;
    
private:
    FontManager() = default;
    ~FontManager() = default;
    
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<FontAtlas>> fontCache;
    
    bool loadFontFile(const std::string& fontPath, std::vector<uint8_t>& fontData);
    bool createFontAtlasTexture(const std::vector<uint8_t>& atlasData, uint32_t width, uint32_t height, 
                               std::shared_ptr<Texture>& texture);
};

} // namespace GameEngine

#endif // FONT_MANAGER_H
