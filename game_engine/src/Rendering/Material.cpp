#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/ShaderManager.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureManager.h"
#include "Rendering/LightingManager.h"
#include <iostream>
#include <vector>

#ifdef LINUX_BUILD
    #include <filesystem>
    #include "Editor/FileDialog.h"
#endif

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

Material::Material()
    : shader(nullptr)
    , color(1.0f, 1.0f, 1.0f)
    , metallic(0.0f)
    , roughness(0.5f)
    , reflectionStrength(0.0f)
    , blendMode(BlendMode::Opaque)
    , diffuseTexture(nullptr)
    , normalTexture(nullptr)
    , armTexture(nullptr)
    , diffuseTexturePath("")
    , normalTexturePath("")
    , armTexturePath("")
    , shaderVertexPathLinux("")
    , shaderFragmentPathLinux("")
    , shaderVertexPathVita("")
    , shaderFragmentPathVita("")
{
}

Material::Material(std::shared_ptr<Shader> materialShader)
    : shader(materialShader)
    , color(1.0f, 1.0f, 1.0f)
    , metallic(0.0f)
    , roughness(0.5f)
    , reflectionStrength(0.0f)
    , blendMode(BlendMode::Opaque)
    , diffuseTexture(nullptr)
    , normalTexture(nullptr)
    , armTexture(nullptr)
    , diffuseTexturePath("")
    , normalTexturePath("")
    , armTexturePath("")
    , shaderVertexPathLinux("")
    , shaderFragmentPathLinux("")
    , shaderVertexPathVita("")
    , shaderFragmentPathVita("")
{
}

Material::~Material() {
}

void Material::setShader(std::shared_ptr<Shader> materialShader) {
    shader = materialShader;
    if (!materialShader || materialShader == Shader::getLightingShader()) {
        shaderVertexPathLinux = "";
        shaderFragmentPathLinux = "";
        shaderVertexPathVita = "";
        shaderFragmentPathVita = "";
    }
}

void Material::setShaderFromPaths(const std::string& vertexPath, const std::string& fragmentPath) {
#ifdef VITA_BUILD
    setShaderFromPathsForPlatform(vertexPath, fragmentPath, "vita");
#else
    setShaderFromPathsForPlatform(vertexPath, fragmentPath, "linux");
#endif
}

void Material::setShaderFromPathsForPlatform(const std::string& vertexPath, const std::string& fragmentPath, const std::string& platform) {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        shaderVertexPathVita = vertexPath;
        shaderFragmentPathVita = fragmentPath;
    } else {
        shaderVertexPathLinux = vertexPath;
        shaderFragmentPathLinux = fragmentPath;
    }
    
#ifdef VITA_BUILD
    bool isCurrentPlatform = (platform == "vita" || platform == "Vita" || platform == "VITA");
#else
    bool isCurrentPlatform = (platform == "linux" || platform == "Linux" || platform == "LINUX");
#endif
    
    if (isCurrentPlatform) {
        auto& shaderManager = ShaderManager::getInstance();
        auto loadedShader = shaderManager.loadShader(vertexPath, fragmentPath);
        
        if (loadedShader && loadedShader->isValid()) {
            shader = loadedShader;
        } else {
            std::cerr << "Failed to load shader from paths: " << vertexPath << ", " << fragmentPath << std::endl;
            shader = Shader::getErrorShader();
        }
    }
}

std::string Material::getShaderVertexPath() const {
#ifdef VITA_BUILD
    return shaderVertexPathVita;
#else
    return shaderVertexPathLinux;
#endif
}

std::string Material::getShaderFragmentPath() const {
#ifdef VITA_BUILD
    return shaderFragmentPathVita;
#else
    return shaderFragmentPathLinux;
#endif
}

std::string Material::getShaderVertexPathForPlatform(const std::string& platform) const {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        return shaderVertexPathVita;
    } else {
        return shaderVertexPathLinux;
    }
}

std::string Material::getShaderFragmentPathForPlatform(const std::string& platform) const {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        return shaderFragmentPathVita;
    } else {
        return shaderFragmentPathLinux;
    }
}

void Material::setShaderVertexPathForPlatform(const std::string& platform, const std::string& path) {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        shaderVertexPathVita = path;
    } else {
        shaderVertexPathLinux = path;
    }
}

void Material::setShaderFragmentPathForPlatform(const std::string& platform, const std::string& path) {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        shaderFragmentPathVita = path;
    } else {
        shaderFragmentPathLinux = path;
    }
}

bool Material::isUsingCustomShader() const {
#ifdef VITA_BUILD
    return !shaderVertexPathVita.empty() && !shaderFragmentPathVita.empty();
#else
    return !shaderVertexPathLinux.empty() && !shaderFragmentPathLinux.empty();
#endif
}

void Material::useDefaultLitShader() {
    shader = Shader::getLightingShader();
    shaderVertexPathLinux = "";
    shaderFragmentPathLinux = "";
    shaderVertexPathVita = "";
    shaderFragmentPathVita = "";
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
    if (texture == nullptr) {
        diffuseTexturePath = "";
    } else if (!path.empty()) {
        diffuseTexturePath = path;
    }
    setTexture("u_DiffuseTexture", texture);
    setBool("u_HasDiffuseTexture", texture != nullptr);
}

void Material::setNormalTexture(std::shared_ptr<Texture> texture, const std::string& path) {
    normalTexture = texture;
    if (texture == nullptr) {
        normalTexturePath = "";
    } else if (!path.empty()) {
        normalTexturePath = path;
    }
    setTexture("u_NormalTexture", texture);
    setBool("u_HasNormalTexture", texture != nullptr);
}

void Material::setARMTexture(std::shared_ptr<Texture> texture, const std::string& path) {
    armTexture = texture;
    if (texture == nullptr) {
        armTexturePath = "";
    } else if (!path.empty()) {
        armTexturePath = path;
    }
    setTexture("u_ARMTexture", texture);
    setBool("u_HasARMTexture", texture != nullptr);
}

void Material::setCameraPosition(const glm::vec3& cameraPos) {
    vec3Properties["u_CameraPos"] = cameraPos;
}

void Material::apply() const {
    if (!shader) {
        const_cast<Material*>(this)->shader = Shader::getLightingShader();
    }
    
#ifdef VITA_BUILD
    if (!shader->isValid() && !shaderVertexPathVita.empty() && !shaderFragmentPathVita.empty()) {
        auto& shaderManager = ShaderManager::getInstance();
        auto loadedShader = shaderManager.loadShader(shaderVertexPathVita, shaderFragmentPathVita);
        if (loadedShader && loadedShader->isValid()) {
            const_cast<Material*>(this)->shader = loadedShader;
        }
    }
#else
    if (!shader->isValid() && !shaderVertexPathLinux.empty() && !shaderFragmentPathLinux.empty()) {
        auto& shaderManager = ShaderManager::getInstance();
        auto loadedShader = shaderManager.loadShader(shaderVertexPathLinux, shaderFragmentPathLinux);
        if (loadedShader && loadedShader->isValid()) {
            const_cast<Material*>(this)->shader = loadedShader;
        }
    }
#endif
    
    if (!shader || !shader->isValid()) {
        return;
    }
    
    shader->use();
    applyProperties();
    
    switch (blendMode) {
        case BlendMode::Opaque:
            glDisable(GL_BLEND);
            break;
        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquation(GL_FUNC_ADD);
            break;
        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);
            break;
    }
    
    auto& shaderManager = ShaderManager::getInstance();
    bool isLit = (shader == Shader::getLightingShader()) || shaderManager.isLitShader(shader);
    
    if (isLit) {
        setupLightingUniforms();
    }
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
        
        if (ImGui::SliderFloat("Reflection Strength", &reflectionStrength, 0.0f, 1.0f)) {
            setReflectionStrength(reflectionStrength);
        }
        
        const char* blendModeNames[] = { "Opaque", "Alpha", "Additive" };
        int currentBlend = static_cast<int>(blendMode);
        if (ImGui::Combo("Blend Mode", &currentBlend, blendModeNames, 3)) {
            setBlendMode(static_cast<BlendMode>(currentBlend));
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
        ImGui::Text("Shader");
        
        bool isDefaultLit = (shader == Shader::getLightingShader());
        bool isCustomShader = isUsingCustomShader();
        
        if (isDefaultLit) {
            ImGui::Text("Type: Default Lit Shader");
#ifdef LINUX_BUILD
            ImGui::Text("Platform: Linux (GLSL)");
#else
            ImGui::Text("Platform: Vita (CG)");
#endif
        } else if (isCustomShader) {
            ImGui::Text("Type: Custom Shader");
            
            if (!shaderVertexPathLinux.empty() && !shaderFragmentPathLinux.empty()) {
                ImGui::Text("Linux (GLSL):");
                ImGui::Text("  Vertex: %s", shaderVertexPathLinux.c_str());
                ImGui::Text("  Fragment: %s", shaderFragmentPathLinux.c_str());
            }
            
            if (!shaderVertexPathVita.empty() && !shaderFragmentPathVita.empty()) {
                ImGui::Text("Vita (CG):");
                ImGui::Text("  Vertex: %s", shaderVertexPathVita.c_str());
                ImGui::Text("  Fragment: %s", shaderFragmentPathVita.c_str());
            }
        } else {
            ImGui::Text("Type: Other");
        }
        
        ImGui::Separator();
        ImGui::Text("Shader Management");
        
        if (ImGui::Button("Create New Shader")) {
            ImGui::OpenPopup("CreateShaderPopup");
        }
        
        if (ImGui::BeginPopup("CreateShaderPopup")) {
            static char shaderNameBuffer[256] = "";
            ImGui::InputText("Shader Name", shaderNameBuffer, sizeof(shaderNameBuffer));
            
            static int shaderTypeSelection = 0;
            const char* shaderTypes[] = { "Lit (PBR)", "Unlit" };
            ImGui::Combo("Shader Type", &shaderTypeSelection, shaderTypes, 2);
            
            static int platformSelection = 0;
            const char* platforms[] = { "Linux (GLSL)", "Vita (CG)" };
            ImGui::Combo("Platform", &platformSelection, platforms, 2);
            
            if (ImGui::Button("Create")) {
                if (strlen(shaderNameBuffer) > 0) {
                    std::string vertexPath, fragmentPath;
                    ShaderType type = (shaderTypeSelection == 0) ? ShaderType::Lit : ShaderType::Unlit;
                    std::string platform = (platformSelection == 0) ? "linux" : "vita";
                    
                    auto& shaderManager = ShaderManager::getInstance();
                    if (shaderManager.createShader(shaderNameBuffer, type, platform, vertexPath, fragmentPath)) {
                        setShaderFromPathsForPlatform(vertexPath, fragmentPath, platform);
                        
#ifdef LINUX_BUILD
                        if (platform == "linux") {
                            shaderManager.registerShaderType(shader, type);
                        }
#else
                        if (platform == "vita") {
                            shaderManager.registerShaderType(shader, type);
                        }
#endif
                        ImGui::CloseCurrentPopup();
                        shaderNameBuffer[0] = '\0';
                    }
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
                shaderNameBuffer[0] = '\0';
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Use Default Lit Shader")) {
            useDefaultLitShader();
        }
        
        if (ImGui::Button("Assign Shader from Files")) {
            ImGui::OpenPopup("AssignShaderPopup");
        }
        
        if (ImGui::BeginPopup("AssignShaderPopup")) {
            static char vertexPathBuffer[512] = "";
            static char fragmentPathBuffer[512] = "";
            static int platformSelection = 0;
            
            const char* platforms[] = { "Linux (GLSL)", "Vita (CG)" };
            ImGui::Combo("Platform", &platformSelection, platforms, 2);
            
            ImGui::Separator();
            
            std::string platformDir = (platformSelection == 0) ? "assets/linux_shaders" : "assets/shaders";
            auto vertexShaders = ShaderManager::discoverShaders(platformDir, ".vert");
            
            if (!vertexShaders.empty()) {
                ImGui::Text("Available Shaders (%s):", platforms[platformSelection]);
                ImGui::BeginChild("ShaderList", ImVec2(0, 150), true);
                
                for (const auto& vertPath : vertexShaders) {
                    std::string fragPath = vertPath;
                    size_t pos = fragPath.find_last_of('.');
                    if (pos != std::string::npos) {
                        fragPath = fragPath.substr(0, pos) + ".frag";
                    }
                    
#ifdef LINUX_BUILD
                    std::string shaderName = std::filesystem::path(vertPath).stem().string();
#else
                    std::string shaderName = vertPath;
                    size_t lastSlash = shaderName.find_last_of("/\\");
                    if (lastSlash != std::string::npos) {
                        shaderName = shaderName.substr(lastSlash + 1);
                    }
                    size_t lastDot = shaderName.find_last_of('.');
                    if (lastDot != std::string::npos) {
                        shaderName = shaderName.substr(0, lastDot);
                    }
#endif
                    
                    if (ImGui::Selectable(shaderName.c_str(), false)) {
                        strncpy(vertexPathBuffer, vertPath.c_str(), sizeof(vertexPathBuffer) - 1);
                        vertexPathBuffer[sizeof(vertexPathBuffer) - 1] = '\0';
                        strncpy(fragmentPathBuffer, fragPath.c_str(), sizeof(fragmentPathBuffer) - 1);
                        fragmentPathBuffer[sizeof(fragmentPathBuffer) - 1] = '\0';
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::Text("No shaders found in %s", platformDir.c_str());
            }
            
            ImGui::Separator();
            ImGui::Text("Or specify paths manually:");
            
            ImGui::InputText("Vertex Shader Path", vertexPathBuffer, sizeof(vertexPathBuffer));
            ImGui::InputText("Fragment Shader Path", fragmentPathBuffer, sizeof(fragmentPathBuffer));
            
            if (ImGui::Button("Browse Vertex...")) {
#ifdef LINUX_BUILD
                std::string filter = (platformSelection == 0) ? "*.vert" : "*.vert";
                std::string selectedPath = FileDialog::openFileDialog("Select Vertex Shader", filter);
                if (FileDialog::isValidResult(selectedPath)) {
                    strncpy(vertexPathBuffer, selectedPath.c_str(), sizeof(vertexPathBuffer) - 1);
                    vertexPathBuffer[sizeof(vertexPathBuffer) - 1] = '\0';
                }
#endif
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Browse Fragment...")) {
#ifdef LINUX_BUILD
                std::string filter = (platformSelection == 0) ? "*.frag" : "*.frag";
                std::string selectedPath = FileDialog::openFileDialog("Select Fragment Shader", filter);
                if (FileDialog::isValidResult(selectedPath)) {
                    strncpy(fragmentPathBuffer, selectedPath.c_str(), sizeof(fragmentPathBuffer) - 1);
                    fragmentPathBuffer[sizeof(fragmentPathBuffer) - 1] = '\0';
                }
#endif
            }
            
            if (ImGui::Button("Assign")) {
                if (strlen(vertexPathBuffer) > 0 && strlen(fragmentPathBuffer) > 0) {
                    std::string platform = (platformSelection == 0) ? "linux" : "vita";
                    setShaderFromPathsForPlatform(vertexPathBuffer, fragmentPathBuffer, platform);
                    
                    auto& shaderManager = ShaderManager::getInstance();
                    if (shader && shader->isValid()) {
                        shaderManager.registerShaderType(shader, ShaderType::Lit);
                    }
                    ImGui::CloseCurrentPopup();
                    vertexPathBuffer[0] = '\0';
                    fragmentPathBuffer[0] = '\0';
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
                vertexPathBuffer[0] = '\0';
                fragmentPathBuffer[0] = '\0';
            }
            
            ImGui::EndPopup();
        }
        
        if (!textureProperties.empty()) {
            ImGui::Separator();
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
    shader->setVec3("diffuseColor", color);
    shader->setVec3("u_Color", color);
    
    auto& shaderManager = ShaderManager::getInstance();
    bool isLit = (shader == Shader::getLightingShader()) || shaderManager.isLitShader(shader);
    
    if (isLit) {
        shader->setFloat("u_Metallic", metallic);
        shader->setFloat("u_Roughness", roughness);
        shader->setFloat("u_ReflectionStrength", reflectionStrength);
        
        bool hasEnvMap = false;
        auto envIt = textureProperties.find("u_EnvironmentMap");
        if (envIt != textureProperties.end() && envIt->second && envIt->second->isCubemap()) {
            hasEnvMap = true;
        }
        shader->setBool("u_HasEnvironmentMap", hasEnvMap);
    }
    
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
            if ((prop.first == "skybox" || prop.first == "u_EnvironmentMap") && prop.second->isCubemap()) {
                prop.second->bindCubemap(textureUnit);
            } else {
                prop.second->bind(textureUnit);
            }
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
