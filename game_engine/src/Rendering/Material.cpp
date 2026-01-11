#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureManager.h"
#include "Rendering/LightingManager.h"
#include <iostream>

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

Material::Material()
    : shader(nullptr)
    , color(1.0f, 1.0f, 1.0f)
    , metallic(0.0f)
    , roughness(0.5f)
    , diffuseTexture(nullptr)
    , normalTexture(nullptr)
    , armTexture(nullptr)
    , diffuseTexturePath("")
    , normalTexturePath("")
    , armTexturePath("")
{
}

Material::Material(std::shared_ptr<Shader> materialShader)
    : shader(materialShader)
    , color(1.0f, 1.0f, 1.0f)
    , metallic(0.0f)
    , roughness(0.5f)
    , diffuseTexture(nullptr)
    , normalTexture(nullptr)
    , armTexture(nullptr)
    , diffuseTexturePath("")
    , normalTexturePath("")
    , armTexturePath("")
{
}

Material::~Material() {
}

void Material::setShader(std::shared_ptr<Shader> materialShader) {
    shader = materialShader;
}

void Material::setFloat(const std::string& name, float value) {
    floatProperties[name] = value;
}

void Material::setInt(const std::string& name, int value) {
    intProperties[name] = value;
}

void Material::setBool(const std::string& name, bool value) {
    boolProperties[name] = value;
}

void Material::setVec2(const std::string& name, const glm::vec2& value) {
    vec2Properties[name] = value;
}

void Material::setVec3(const std::string& name, const glm::vec3& value) {
    vec3Properties[name] = value;
}

void Material::setVec4(const std::string& name, const glm::vec4& value) {
    vec4Properties[name] = value;
}

void Material::setMat3(const std::string& name, const glm::mat3& value) {
    mat3Properties[name] = value;
}

void Material::setMat4(const std::string& name, const glm::mat4& value) {
    mat4Properties[name] = value;
}

void Material::setTexture(const std::string& name, std::shared_ptr<Texture> texture) {
    textureProperties[name] = texture;
}

void Material::setDiffuseTexture(std::shared_ptr<Texture> texture, const std::string& path) {
    diffuseTexture = texture;
    if (!path.empty()) {
        diffuseTexturePath = path;
    }
    setTexture("u_DiffuseTexture", texture);
    setBool("u_HasDiffuseTexture", texture != nullptr);
}

void Material::setNormalTexture(std::shared_ptr<Texture> texture, const std::string& path) {
    normalTexture = texture;
    if (!path.empty()) {
        normalTexturePath = path;
    }
    setTexture("u_NormalTexture", texture);
    setBool("u_HasNormalTexture", texture != nullptr);
}

void Material::setARMTexture(std::shared_ptr<Texture> texture, const std::string& path) {
    armTexture = texture;
    if (!path.empty()) {
        armTexturePath = path;
    }
    setTexture("u_ARMTexture", texture);
    setBool("u_HasARMTexture", texture != nullptr);
}

void Material::setCameraPosition(const glm::vec3& cameraPos) {
    vec3Properties["u_CameraPos"] = cameraPos;
}

void Material::apply() const {
    const_cast<Material*>(this)->shader = Shader::getLightingShader();
    
    if (!shader || !shader->isValid()) {
        return;
    }
    
    shader->use();
    applyProperties();
    setupLightingUniforms();
}

void Material::drawInspector() {
#ifdef EDITOR_BUILD
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::ColorEdit3("Color", &color.x)) {
            setColor(color);
        }
        
        if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
            setFloat("u_Metallic", metallic);
        }
        
        if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
            setFloat("u_Roughness", roughness);
        }
        
        ImGui::Separator();
        ImGui::Text("Textures");
        
        if (ImGui::BeginCombo("Diffuse Texture", diffuseTexture ? "Loaded" : "None")) {
            if (ImGui::Selectable("None", !diffuseTexture)) {
                setDiffuseTexture(nullptr);
            }
            
            auto& textureManager = TextureManager::getInstance();
            auto availableTextures = textureManager.getAvailableTextures();
            
            for (const auto& texturePath : availableTextures) {
                if (texturePath.find("_diff") != std::string::npos || 
                    texturePath.find("diffuse") != std::string::npos) {
                    if (ImGui::Selectable(texturePath.c_str(), diffuseTexture && 
                        texturePath == "u_DiffuseTexture")) {
                        auto texture = textureManager.getTexture(texturePath);
                        setDiffuseTexture(texture, texturePath);
                    }
                }
            }
            ImGui::EndCombo();
        }
        
        if (ImGui::BeginCombo("Normal Texture", normalTexture ? "Loaded" : "None")) {
            if (ImGui::Selectable("None", !normalTexture)) {
                setNormalTexture(nullptr);
            }
            
            auto& textureManager = TextureManager::getInstance();
            auto availableTextures = textureManager.getAvailableTextures();
            
            for (const auto& texturePath : availableTextures) {
                if (texturePath.find("_nor") != std::string::npos || 
                    texturePath.find("normal") != std::string::npos) {
                    if (ImGui::Selectable(texturePath.c_str(), normalTexture && 
                        texturePath == "u_NormalTexture")) {
                        auto texture = textureManager.getTexture(texturePath);
                        setNormalTexture(texture, texturePath);
                    }
                }
            }
            ImGui::EndCombo();
        }
        
        if (ImGui::BeginCombo("ARM Texture", armTexture ? "Loaded" : "None")) {
            if (ImGui::Selectable("None", !armTexture)) {
                setARMTexture(nullptr);
            }
            
            auto& textureManager = TextureManager::getInstance();
            auto availableTextures = textureManager.getAvailableTextures();
            
            for (const auto& texturePath : availableTextures) {
                if (texturePath.find("_arm") != std::string::npos || 
                    texturePath.find("arm") != std::string::npos) {
                    if (ImGui::Selectable(texturePath.c_str(), armTexture && 
                        texturePath == "u_ARMTexture")) {
                        auto texture = textureManager.getTexture(texturePath);
                        setARMTexture(texture, texturePath);
                    }
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::Separator();
        ImGui::Text("Shader: %s", shader ? "Loaded" : "None");
        
        if (!textureProperties.empty()) {
            ImGui::Text("Texture Properties: %zu", textureProperties.size());
        }
    }
#endif
}

std::shared_ptr<Material> Material::getDefaultMaterial() {
    static std::shared_ptr<Material> defaultMaterial = nullptr;
    
    if (!defaultMaterial) {
        defaultMaterial = std::make_shared<Material>();
        defaultMaterial->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
    }
    
    return defaultMaterial;
}

std::shared_ptr<Material> Material::getErrorMaterial() {
    static std::shared_ptr<Material> errorMaterial = nullptr;
    
    if (!errorMaterial) {
        errorMaterial = std::make_shared<Material>();
        errorMaterial->setShader(Shader::getErrorShader());
        errorMaterial->setColor(glm::vec3(1.0f, 0.0f, 1.0f));
    }
    
    return errorMaterial;
}

void Material::applyProperties() const {
    if (!shader) return;
    
    shader->setVec3("u_DiffuseColor", color);
    
    for (const auto& prop : floatProperties) {
        shader->setFloat(prop.first, prop.second);
    }
    
    for (const auto& prop : intProperties) {
        shader->setInt(prop.first, prop.second);
    }
    
    for (const auto& prop : boolProperties) {
        shader->setBool(prop.first, prop.second);
    }
    
    for (const auto& prop : vec2Properties) {
        shader->setVec2(prop.first, prop.second);
    }
    
    for (const auto& prop : vec3Properties) {
        shader->setVec3(prop.first, prop.second);
    }
    
    for (const auto& prop : vec4Properties) {
        shader->setVec4(prop.first, prop.second);
    }
    
    for (const auto& prop : mat3Properties) {
        shader->setMat3(prop.first, prop.second);
    }
    
    for (const auto& prop : mat4Properties) {
        shader->setMat4(prop.first, prop.second);
    }
    
    int textureUnit = 0;
    
    if (diffuseTexture) {
        diffuseTexture->bind(textureUnit);
        shader->setInt("u_DiffuseTexture", textureUnit);
        shader->setBool("u_HasDiffuseTexture", true);
        textureUnit++;
    } else {
        shader->setBool("u_HasDiffuseTexture", false);
    }
    
    if (normalTexture) {
        normalTexture->bind(textureUnit);
        shader->setInt("u_NormalTexture", textureUnit);
        shader->setBool("u_HasNormalTexture", true);
        textureUnit++;
    } else {
        shader->setBool("u_HasNormalTexture", false);
    }
    
    if (armTexture) {
        armTexture->bind(textureUnit);
        shader->setInt("u_ARMTexture", textureUnit);
        shader->setBool("u_HasARMTexture", true);
        textureUnit++;
    } else {
        shader->setBool("u_HasARMTexture", false);
    }
    
    for (const auto& prop : textureProperties) {
        if (prop.second) {
            prop.second->bind(textureUnit);
            shader->setInt(prop.first, textureUnit);
            textureUnit++;
        }
    }
}

void Material::setupLightingUniforms() const {
    if (!shader) return;
    
    if (floatProperties.find("Kd") == floatProperties.end()) {
        shader->setFloat("Kd", 1.0f);
    }
    
    auto& lightingManager = LightingManager::getInstance();
    auto lightDataArray = lightingManager.getLightDataArray();
    
    shader->setInt("u_NumLights", static_cast<int>(lightingManager.getActiveLightCount()));
    
    for (size_t i = 0; i < lightDataArray.size(); ++i) {
        const auto& lightData = lightDataArray[i];
        std::string lightIndex = "u_Lights[" + std::to_string(i) + "]";
        
        shader->setVec4(lightIndex + ".position", lightData.position);
        shader->setVec4(lightIndex + ".direction", lightData.direction);
        shader->setVec4(lightIndex + ".color", lightData.color);
        shader->setVec4(lightIndex + ".params", lightData.params);
        shader->setVec4(lightIndex + ".attenuation", lightData.attenuation);
    }
    
    auto cameraPosIt = vec3Properties.find("u_CameraPos");
    if (cameraPosIt != vec3Properties.end()) {
        shader->setVec3("u_CameraPos", cameraPosIt->second);
    } else {
        shader->setVec3("u_CameraPos", glm::vec3(0.0f, 0.0f, 5.0f));
    }
}

} // namespace GameEngine
