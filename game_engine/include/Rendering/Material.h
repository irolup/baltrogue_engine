#ifndef MATERIAL_H
#define MATERIAL_H

#include <string>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Platform.h"

namespace GameEngine {

class Shader;
class Texture;

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
    
    float getMetallic() const { return metallic; }
    void setMetallic(float m) { metallic = m; setFloat("u_Metallic", metallic); }
    
    float getRoughness() const { return roughness; }
    void setRoughness(float r) { roughness = r; setFloat("u_Roughness", roughness); }
    
    float getReflectionStrength() const { return reflectionStrength; }
    void setReflectionStrength(float r) { reflectionStrength = r; setFloat("u_ReflectionStrength", reflectionStrength); }
    
    std::shared_ptr<Texture> getDiffuseTexture() const { return diffuseTexture; }
    void setDiffuseTexture(std::shared_ptr<Texture> texture, const std::string& path = "");
    
    std::shared_ptr<Texture> getNormalTexture() const { return normalTexture; }
    void setNormalTexture(std::shared_ptr<Texture> texture, const std::string& path = "");
    
    std::shared_ptr<Texture> getARMTexture() const { return armTexture; }
    void setARMTexture(std::shared_ptr<Texture> texture, const std::string& path = "");
    
    std::string getDiffuseTexturePath() const { return diffuseTexturePath; }
    void setDiffuseTexturePath(const std::string& path) { diffuseTexturePath = path; }
    
    std::string getNormalTexturePath() const { return normalTexturePath; }
    void setNormalTexturePath(const std::string& path) { normalTexturePath = path; }
    
    std::string getARMTexturePath() const { return armTexturePath; }
    void setARMTexturePath(const std::string& path) { armTexturePath = path; }
    
    bool hasDiffuseTexture() const { return diffuseTexture != nullptr; }
    bool hasNormalTexture() const { return normalTexture != nullptr; }
    bool hasARMTexture() const { return armTexture != nullptr; }
    
    void drawInspector();
    
    void setupLightingUniforms() const;
    void setCameraPosition(const glm::vec3& cameraPos);
    
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
    
    std::shared_ptr<Texture> diffuseTexture;
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> armTexture;
    
    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string armTexturePath;
    
    void applyProperties() const;
};

} // namespace GameEngine

#endif // MATERIAL_H
