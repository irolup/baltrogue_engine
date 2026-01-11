#include "Rendering/FontManager.h"
#include "Rendering/Texture.h"
#include <fstream>
#include <iostream>
#include <sstream>

// Include stb_truetype
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../vendor/stb/stb_truetype.h"

namespace GameEngine {

FontManager& FontManager::getInstance() {
    static FontManager instance;
    return instance;
}

std::shared_ptr<FontAtlas> FontManager::loadFont(const std::string& fontPath, float fontSize, 
                                                uint32_t atlasWidth, uint32_t atlasHeight,
                                                uint32_t charsToInclude, uint32_t firstCharCodePoint) {
    std::string key = getFontKey(fontPath, fontSize);
    
    auto it = fontCache.find(key);
    if (it != fontCache.end()) {
        return it->second;
    }
    
    auto atlas = std::make_shared<FontAtlas>();
    if (generateFontAtlas(fontPath, fontSize, *atlas, atlasWidth, atlasHeight, charsToInclude, firstCharCodePoint)) {
        fontCache[key] = atlas;
        return atlas;
    }
    
    return nullptr;
}

std::shared_ptr<FontAtlas> FontManager::getFont(const std::string& fontPath, float fontSize) {
    std::string key = getFontKey(fontPath, fontSize);
    auto it = fontCache.find(key);
    return (it != fontCache.end()) ? it->second : nullptr;
}

bool FontManager::generateFontAtlas(const std::string& fontPath, float fontSize, FontAtlas& atlas,
                                  uint32_t atlasWidth, uint32_t atlasHeight,
                                  uint32_t charsToInclude, uint32_t firstCharCodePoint) {
    std::vector<uint8_t> fontData;
    if (!loadFontFile(fontPath, fontData)) {
        std::cerr << "Failed to load font file: " << fontPath << std::endl;
        return false;
    }
    
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0)) {
        std::cerr << "Failed to initialize font: " << fontPath << std::endl;
        return false;
    }
    
    int fontCount = stbtt_GetNumberOfFonts(fontData.data());
    std::cout << "Font file: " << fontPath << " has " << fontCount << " fonts" << std::endl;
    
    std::vector<uint8_t> atlasData(atlasWidth * atlasHeight);
    
    stbtt_pack_context packContext;
    if (!stbtt_PackBegin(&packContext, atlasData.data(), atlasWidth, atlasHeight, 0, 1, nullptr)) {
        std::cerr << "Failed to begin font packing" << std::endl;
        return false;
    }
    
    atlas.packedChars.resize(charsToInclude);
    
    if (!stbtt_PackFontRange(&packContext, fontData.data(), 0, fontSize, 
                            firstCharCodePoint, charsToInclude, atlas.packedChars.data())) {
        std::cerr << "Failed to pack font range for font: " << fontPath << " (size: " << fontSize << ")" << std::endl;
        std::cerr << "Atlas size: " << atlasWidth << "x" << atlasHeight << ", Characters: " << charsToInclude << std::endl;
        stbtt_PackEnd(&packContext);
        return false;
    }
    
    stbtt_PackEnd(&packContext);
    
    std::cout << "Successfully generated font atlas for: " << fontPath << " (size: " << fontSize << ")" << std::endl;
    
    atlas.alignedQuads.resize(charsToInclude);
    for (uint32_t i = 0; i < charsToInclude; i++) {
        float unusedX, unusedY;
        stbtt_GetPackedQuad(atlas.packedChars.data(), atlasWidth, atlasHeight, i,
                           &unusedX, &unusedY, &atlas.alignedQuads[i], 0);
    }
    
    if (!createFontAtlasTexture(atlasData, atlasWidth, atlasHeight, atlas.texture)) {
        std::cerr << "Failed to create font atlas texture" << std::endl;
        return false;
    }
    
    atlas.atlasWidth = atlasWidth;
    atlas.atlasHeight = atlasHeight;
    atlas.fontSize = static_cast<uint32_t>(fontSize);
    atlas.charsToInclude = charsToInclude;
    atlas.firstCharCodePoint = firstCharCodePoint;
    
    std::cout << "Successfully generated font atlas for: " << fontPath 
              << " (size: " << fontSize << ")" << std::endl;
    
    return true;
}

void FontManager::clearCache() {
    fontCache.clear();
}

void FontManager::removeFont(const std::string& fontPath, float fontSize) {
    std::string key = getFontKey(fontPath, fontSize);
    fontCache.erase(key);
}

std::string FontManager::getFontKey(const std::string& fontPath, float fontSize) const {
    std::stringstream ss;
    ss << fontPath << "_" << fontSize;
    return ss.str();
}

bool FontManager::isFontLoaded(const std::string& fontPath, float fontSize) const {
    std::string key = getFontKey(fontPath, fontSize);
    return fontCache.find(key) != fontCache.end();
}

bool FontManager::loadFontFile(const std::string& fontPath, std::vector<uint8_t>& fontData) {
    std::ifstream file(fontPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open font file: " << fontPath << std::endl;
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    fontData.resize(fileSize);
    file.read(reinterpret_cast<char*>(fontData.data()), fileSize);
    
    if (!file.good()) {
        std::cerr << "Failed to read font file: " << fontPath << std::endl;
        return false;
    }
    
    return true;
}

bool FontManager::createFontAtlasTexture(const std::vector<uint8_t>& atlasData, uint32_t width, uint32_t height, 
                                       std::shared_ptr<Texture>& texture) {
    texture = std::make_shared<Texture>();
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, atlasData.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    texture->setTextureID(textureID);
    
    return true;
}

} // namespace GameEngine
