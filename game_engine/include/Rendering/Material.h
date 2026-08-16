#ifndef MATERIAL_H
#define MATERIAL_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <utility>
#include <cstdint>
#include <glm/glm.hpp>
#include "Platform.h"
#include "Rendering/RenderTypes.h"

namespace GameEngine {

class Shader;
class Texture;

enum class BlendMode {
    Opaque,
    Alpha,
    Additive
};

struct MaterialOverride {
    bool overrideBaseColor = false;
    glm::vec3 baseColor{1.0f, 1.0f, 1.0f};

    bool overrideMetallic = false;
    float metallic = 0.0f;

    bool overrideRoughness = false;
    float roughness = 0.5f;

    bool overrideReflectionStrength = false;
    float reflectionStrength = 0.0f;

    bool overrideOpacity = false;
    float opacity = 1.0f;

    bool overrideAlphaCutoff = false;
    float alphaCutoff = 0.0f;

    bool overrideBlendMode = false;
    BlendMode blendMode = BlendMode::Opaque;

    bool overrideDoubleSided = false;
    bool doubleSided = false;

    bool overrideUVTransform = false;
    glm::vec2 uvScale{1.0f, 1.0f};
    glm::vec2 uvOffset{0.0f, 0.0f};

    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string armTexturePath;

    std::string shaderVertexPathLinux;
    std::string shaderFragmentPathLinux;
    std::string shaderVertexPathVita;
    std::string shaderFragmentPathVita;
    std::string shaderVertexPathVulkan;
    std::string shaderFragmentPathVulkan;

    /** True when nothing is overridden, so the base material can be used as-is. */
    bool isEmpty() const;
};

class Material {
public:
    Material();
    Material(std::shared_ptr<Shader> shader);
    ~Material();
    
    void setShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> getShader() const { return shader; }
    
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    void setBool(const std::string& name, bool value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat3(const std::string& name, const glm::mat3& value);
    void setMat4(const std::string& name, const glm::mat4& value);
    void setTexture(const std::string& name, std::shared_ptr<Texture> texture);
    
    void apply() const;
    
    glm::vec3 getColor() const { return color; }
    void setColor(const glm::vec3& c) { 
        color = c; 
        setVec3("diffuseColor", color);
        setVec3("u_Color", color);
    }
    glm::vec3 getColorLinear() const;
    
    float getMetallic() const { return metallic; }
    void setMetallic(float m) { metallic = m; setFloat("u_Metallic", metallic); }
    
    float getRoughness() const { return roughness; }
    void setRoughness(float r) { roughness = r; setFloat("u_Roughness", roughness); }
    
    float getReflectionStrength() const { return reflectionStrength; }
    void setReflectionStrength(float r) { reflectionStrength = r; setFloat("u_ReflectionStrength", reflectionStrength); }
    
    BlendMode getBlendMode() const { return blendMode; }
    void setBlendMode(BlendMode mode);
    
    float getOpacity() const { return opacity; }
    void setOpacity(float o);

    float getAlphaCutoff() const { return alphaCutoff; }
    void setAlphaCutoff(float cutoff);

    bool getDoubleSided() const { return doubleSided; }
    void setDoubleSided(bool enabled) { doubleSided = enabled; bumpRevision(); }

    bool getDepthWrite() const { return depthWrite; }
    void setDepthWrite(bool write) { depthWrite = write; bumpRevision(); }

    glm::vec2 getUVScale() const { return uvScale; }
    void setUVScale(const glm::vec2& scale) { uvScale = scale; setVec2("u_UVScale", uvScale); }

    glm::vec2 getUVOffset() const { return uvOffset; }
    void setUVOffset(const glm::vec2& offset) { uvOffset = offset; setVec2("u_UVOffset", uvOffset); }

    void resetUVTransform() { setUVScale(glm::vec2(1.0f, 1.0f)); setUVOffset(glm::vec2(0.0f, 0.0f)); }
    
    std::shared_ptr<Texture> getDiffuseTexture() const { return diffuseTexture; }
    void setDiffuseTexture(std::shared_ptr<Texture> texture, const std::string& path = "");
    
    std::shared_ptr<Texture> getNormalTexture() const { return normalTexture; }
    void setNormalTexture(std::shared_ptr<Texture> texture, const std::string& path = "");
    
    std::shared_ptr<Texture> getARMTexture() const { return armTexture; }
    void setARMTexture(std::shared_ptr<Texture> texture, const std::string& path = "");

    std::shared_ptr<Texture> getEnvironmentTexture() const { return environmentTexture; }
    void setEnvironmentTexture(std::shared_ptr<Texture> texture, const std::string& path = "");
    
    std::string getDiffuseTexturePath() const { return diffuseTexturePath; }
    void setDiffuseTexturePath(const std::string& path) { diffuseTexturePath = path; bumpRevision(); }

    std::string getNormalTexturePath() const { return normalTexturePath; }
    void setNormalTexturePath(const std::string& path) { normalTexturePath = path; bumpRevision(); }

    std::string getARMTexturePath() const { return armTexturePath; }
    void setARMTexturePath(const std::string& path) { armTexturePath = path; bumpRevision(); }

    std::string getEnvironmentTexturePath() const { return environmentTexturePath; }
    void setEnvironmentTexturePath(const std::string& path) { environmentTexturePath = path; bumpRevision(); }
    
    bool hasDiffuseTexture() const {
    #ifndef ENABLE_VULKAN
        return diffuseTexture != nullptr;
    #else
        return !diffuseTexturePath.empty();
    #endif
    }
    bool hasNormalTexture() const {
    #ifndef ENABLE_VULKAN
        return normalTexture != nullptr;
    #else
        return !normalTexturePath.empty();
    #endif
    }
    bool hasARMTexture() const {
    #ifndef ENABLE_VULKAN
        return armTexture != nullptr;
    #else
        return !armTexturePath.empty();
    #endif
    }
    bool hasEnvironmentTexture() const {
    #ifndef ENABLE_VULKAN
        return environmentTexture != nullptr;
    #else
        return !environmentTexturePath.empty();
    #endif
    }
    
    void setShaderFromPaths(const std::string& vertexPath, const std::string& fragmentPath);
    void setShaderFromPathsForPlatform(const std::string& vertexPath, const std::string& fragmentPath, const std::string& platform);
    
    std::string getShaderVertexPath() const;
    std::string getShaderFragmentPath() const;
    std::string getShaderVertexPathForPlatform(const std::string& platform) const;
    std::string getShaderFragmentPathForPlatform(const std::string& platform) const;

    void setShaderVertexPathForPlatform(const std::string& platform, const std::string& path);
    void setShaderFragmentPathForPlatform(const std::string& platform, const std::string& path);

    bool isUsingCustomShader() const;
    void useDefaultLitShader();

    // Copy sharing this material's shader and textures, safe to override per instance
    std::shared_ptr<Material> clone() const;

    void applyOverride(const MaterialOverride& overrides);

    // Bumped by every property change. Backends that cache GPU state per material
    // compare it to know when their copy went stale
    uint32_t getRevision() const { return revision; }

#ifdef ENABLE_VULKAN
    bool isBeamMaterial() const;
    VulkanShaderPipelineKind getVulkanShaderPipelineKind() const;
    std::string getVulkanVertexSpvPath() const;
    std::string getVulkanFragmentSpvPath() const;
    std::string getVulkanShaderPipelineKey() const;
    std::string getCustomTexturePathByUniformName(const std::string& uniformName) const;
    bool hasCustomTextureUniform(const std::string& uniformName) const;
#endif
    
    using CustomTextureUniform = std::pair<std::string, std::string>;
    const std::vector<CustomTextureUniform>& getCustomTextureUniforms() const { return customTextureUniforms; }
    void addCustomTextureUniform(const std::string& uniformName, const std::string& texturePath);
    void removeCustomTextureUniform(const std::string& uniformName);
    void setCustomTextureUniformPath(const std::string& uniformName, const std::string& texturePath);
    
    void drawInspector();

    // Editor shader picker, shared with MaterialComponent's inspector
    static bool drawShaderAssignPopup(const char* popupId,
                                      std::string& outVertexPath,
                                      std::string& outFragmentPath,
                                      std::string& outPlatform);

    void setupLightingUniforms() const;
    void setCameraPosition(const glm::vec3& cameraPos);
    void setupShadowUniforms() const;
    
    static std::shared_ptr<Material> getDefaultMaterial();
    static std::shared_ptr<Material> getErrorMaterial();
    
private:
    std::shared_ptr<Shader> shader;
    
    std::unordered_map<std::string, float> floatProperties;
    std::unordered_map<std::string, int> intProperties;
    std::unordered_map<std::string, bool> boolProperties;
    std::unordered_map<std::string, glm::vec2> vec2Properties;
    std::unordered_map<std::string, glm::vec3> vec3Properties;
    std::unordered_map<std::string, glm::vec4> vec4Properties;
    std::unordered_map<std::string, glm::mat3> mat3Properties;
    std::unordered_map<std::string, glm::mat4> mat4Properties;
    std::unordered_map<std::string, std::shared_ptr<Texture>> textureProperties;
    
    glm::vec3 color;
    float metallic;
    float roughness;
    float reflectionStrength;
    float opacity = 1.0f;
    float alphaCutoff = 0.0f; // >0 enables alpha-test discard
    BlendMode blendMode;
    bool depthWrite = true;
    bool doubleSided = false;
    glm::vec2 uvScale{1.0f, 1.0f};
    glm::vec2 uvOffset{0.0f, 0.0f};
    
    std::shared_ptr<Texture> diffuseTexture;
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> armTexture;
    std::shared_ptr<Texture> environmentTexture;
    
    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string armTexturePath;
    std::string environmentTexturePath;
    
    std::string shaderVertexPathLinux;
    std::string shaderFragmentPathLinux;
    std::string shaderVertexPathVita;
    std::string shaderFragmentPathVita;
    std::string shaderVertexPathVulkan;
    std::string shaderFragmentPathVulkan;
    
    std::vector<CustomTextureUniform> customTextureUniforms;

    // Copying a material copies a stale revision, so the counter is global
    uint32_t revision = 0;
    static uint32_t nextRevision;
    void bumpRevision() { revision = ++nextRevision; }

    void applyProperties() const;
    void rebindTextureFromPath(const std::string& path,
                               void (Material::*setTextureFn)(std::shared_ptr<Texture>, const std::string&));
};

} // namespace GameEngine

#endif // MATERIAL_H
