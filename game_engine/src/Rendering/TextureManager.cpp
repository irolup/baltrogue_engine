#include "Rendering/TextureManager.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef LINUX_BUILD
    #include <filesystem>
#else
    #include <psp2/io/fcntl.h>  // Vita SDK file I/O
    #include <psp2/io/stat.h>   // Vita SDK file stat
#endif

namespace GameEngine {

TextureManager& TextureManager::getInstance() {
    static TextureManager instance;
    return instance;
}

std::shared_ptr<Texture> TextureManager::loadTexture(const std::string& filepath) {
    auto it = textureCache.find(filepath);
    if (it != textureCache.end()) {
        return it->second;
    }
    
    auto texture = std::make_shared<Texture>();
    if (texture->loadFromFile(filepath)) {
        textureCache[filepath] = texture;
        std::cout << "Cached texture: " << filepath << std::endl;
        return texture;
    } else {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
        return nullptr;
    }
}

std::shared_ptr<Texture> TextureManager::getTexture(const std::string& filepath) {
    auto it = textureCache.find(filepath);
    if (it != textureCache.end()) {
        return it->second;
    }
    
    return loadTexture(filepath);
}

std::vector<std::string> TextureManager::discoverTextures(const std::string& directory) {
    discoveredTextures.clear();
    
    if (discoverTexturesInDirectory(directory)) {
        std::cout << "Discovered " << discoveredTextures.size() << " textures in " << directory << std::endl;
    }
    
    return discoveredTextures;
}

std::vector<std::string> TextureManager::discoverAllTextures(const std::string& rootDirectory) {
    discoveredTextures.clear();
    
    if (discoverTexturesRecursively(rootDirectory)) {
        std::cout << "Discovered " << discoveredTextures.size() << " textures recursively in " << rootDirectory << std::endl;
    }
    
    return discoveredTextures;
}

std::vector<std::string> TextureManager::getAvailableTextures() const {
    return discoveredTextures;
}

std::shared_ptr<Texture> TextureManager::getTextureByType(const std::string& basePath, TextureType type) {
    std::string suffix = getTextureTypeSuffix(type);
    std::string filepath = basePath + "_" + suffix + ".png";
    
    return getTexture(filepath);
}

std::string TextureManager::getTextureTypeSuffix(TextureType type) const {
    switch (type) {
        case TextureType::DIFFUSE: return "diff";
        case TextureType::NORMAL: return "nor_gl";
        case TextureType::ARM: return "arm";
        case TextureType::SPECULAR: return "spec";
        case TextureType::EMISSIVE: return "emit";
        default: return "diff";
    }
}

bool TextureManager::hasTexture(const std::string& filepath) const {
    return textureCache.find(filepath) != textureCache.end();
}

void TextureManager::clearCache() {
    textureCache.clear();
    discoveredTextures.clear();
}

bool TextureManager::discoverTexturesInDirectory(const std::string& directory) {
#ifdef LINUX_BUILD
    try {
        std::filesystem::path dirPath(directory);
        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
            std::cerr << "Directory does not exist: " << directory << std::endl;
            return false;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
                    discoveredTextures.push_back(entry.path().string());
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error discovering textures: " << e.what() << std::endl;
        return false;
    }
#else
    // Vita: Try to read texture manifest first, fallback to filesystem scanning
    if (readTextureManifest()) {
        std::cout << "Vita: Loaded texture manifest with " << discoveredTextures.size() << " textures" << std::endl;
        return true;
    }
    
    // Fallback: Use filesystem scanning during development
    std::cout << "Vita: Manifest not found, scanning filesystem for development..." << std::endl;
    // For Vita development, we'll use a simple directory listing approach
    // This is a simplified version that works without std::filesystem
    return false; // Will fall back to empty list
#endif
}

bool TextureManager::discoverTexturesRecursively(const std::string& rootDirectory) {
#ifdef LINUX_BUILD
    try {
        std::filesystem::path rootPath(rootDirectory);
        if (!std::filesystem::exists(rootPath) || !std::filesystem::is_directory(rootPath)) {
            std::cerr << "Root directory does not exist: " << rootDirectory << std::endl;
            return false;
        }
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
                    discoveredTextures.push_back(entry.path().string());
                }
            }
        }
        
        std::cout << "Linux: Discovered " << discoveredTextures.size() << " textures with full paths" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error discovering textures recursively: " << e.what() << std::endl;
        return false;
    }
#else
    // Vita: Try to read texture manifest first, fallback to filesystem scanning
    if (readTextureManifest()) {
        std::cout << "Vita: Loaded texture manifest with " << discoveredTextures.size() << " textures" << std::endl;
        return true;
    }
    
    // Fallback: Use filesystem scanning during development
    std::cout << "Vita: Manifest not found, scanning filesystem for development..." << std::endl;
    // For Vita development, we'll use a simple directory listing approach
    // This is a simplified version that works without std::filesystem
    return false; // Will fall back to empty list
#endif
}

#ifndef LINUX_BUILD
bool TextureManager::readTextureManifest() {
    SceUID fd = sceIoOpen("app0:/textures.txt", SCE_O_RDONLY, 0);
    if (fd < 0) {
        return false;
    }
    
    SceOff fileSize = sceIoLseek(fd, 0, SCE_SEEK_END);
    sceIoLseek(fd, 0, SCE_SEEK_SET);
    
    if (fileSize <= 0) {
        sceIoClose(fd);
        return false;
    }
    
    char* buffer = new char[fileSize + 1];
    int bytesRead = sceIoRead(fd, buffer, fileSize);
    sceIoClose(fd);
    
    if (bytesRead != fileSize) {
        delete[] buffer;
        return false;
    }
    
    buffer[fileSize] = '\0';
    
    std::string content(buffer);
    delete[] buffer;
    
    discoveredTextures.clear();
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (!line.empty()) {
            discoveredTextures.push_back(line);
        }
    }
    
    return !discoveredTextures.empty();
}

bool TextureManager::writeTextureManifest(const std::vector<std::string>& textures) {
    std::ofstream file("textures.txt");
    if (!file.is_open()) {
        std::cerr << "Failed to create texture manifest file" << std::endl;
        return false;
    }
    
    for (const auto& texture : textures) {
        std::string filename = texture;
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }
        file << filename << std::endl;
    }
    
    file.close();
    std::cout << "Generated texture manifest with " << textures.size() << " textures" << std::endl;
    return true;
}
#endif

} // namespace GameEngine
