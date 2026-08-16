#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Rendering/Shader.h"

namespace GameEngine {

enum class ShaderType {
    Lit,
    Unlit
};

class ShaderManager {
public:
    static ShaderManager& getInstance();
    
    bool createShader(const std::string& shaderName, ShaderType type, const std::string& platform, std::string& outVertexPath, std::string& outFragmentPath);
    
    std::shared_ptr<Shader> loadShader(const std::string& vertexPath, const std::string& fragmentPath);
    
    std::shared_ptr<Shader> getShader(const std::string& vertexPath, const std::string& fragmentPath);
    
    bool isLitShader(std::shared_ptr<Shader> shader) const;
    
    void registerShaderType(std::shared_ptr<Shader> shader, ShaderType type);
    
    static std::string getShaderDirectory();
    
    static std::string getShaderDirectory(const std::string& platform);
    
    static std::vector<std::string> discoverShaders(const std::string& directory, const std::string& extension = ".vert");

    // Returns true if the .spv already exists
    static bool compileVulkanGlslToSpv(const std::string& glslPath, std::string* errorOut = nullptr);
    
private:
    ShaderManager() = default;
    ~ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    
    std::string generateLitVertexShader(bool isVita);
    std::string generateLitFragmentShader(bool isVita);
    std::string generateUnlitVertexShader(bool isVita);
    std::string generateUnlitFragmentShader(bool isVita);
    
    bool writeShaderFile(const std::string& filepath, const std::string& content);
    
    std::unordered_map<std::string, std::shared_ptr<Shader>> shaderCache;
    
    std::unordered_map<std::string, ShaderType> shaderTypes;
    
    std::string getShaderCacheKey(const std::string& vertexPath, const std::string& fragmentPath) const;
};

} // namespace GameEngine

#endif // SHADER_MANAGER_H
