#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/ShaderManager.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureManager.h"
#include "Rendering/LightingManager.h"
#include "Rendering/ShadowMap.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>

#ifdef LINUX_BUILD
    #include <filesystem>
    #include "Editor/FileDialog.h"
    #include "Editor/ProjectAssets.h"
#endif

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

uint32_t Material::nextRevision = 0;

bool MaterialOverride::isEmpty() const {
    return !overrideBaseColor
        && !overrideMetallic
        && !overrideRoughness
        && !overrideReflectionStrength
        && !overrideOpacity
        && !overrideAlphaCutoff
        && !overrideBlendMode
        && !overrideDoubleSided
        && !overrideUVTransform
        && diffuseTexturePath.empty()
        && normalTexturePath.empty()
        && armTexturePath.empty()
        && shaderVertexPathLinux.empty()
        && shaderFragmentPathLinux.empty()
        && shaderVertexPathVita.empty()
        && shaderFragmentPathVita.empty()
        && shaderVertexPathVulkan.empty()
        && shaderFragmentPathVulkan.empty();
}

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
    , environmentTexture(nullptr)
    , diffuseTexturePath("")
    , normalTexturePath("")
    , armTexturePath("")
    , environmentTexturePath("")
    , shaderVertexPathLinux("")
    , shaderFragmentPathLinux("")
    , shaderVertexPathVita("")
    , shaderFragmentPathVita("")
    , shaderVertexPathVulkan("")
    , shaderFragmentPathVulkan("")
{
    setVec2("u_UVScale", uvScale);
    setVec2("u_UVOffset", uvOffset);
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
    , environmentTexture(nullptr)
    , diffuseTexturePath("")
    , normalTexturePath("")
    , armTexturePath("")
    , environmentTexturePath("")
    , shaderVertexPathLinux("")
    , shaderFragmentPathLinux("")
    , shaderVertexPathVita("")
    , shaderFragmentPathVita("")
    , shaderVertexPathVulkan("")
    , shaderFragmentPathVulkan("")
{
    setVec2("u_UVScale", uvScale);
    setVec2("u_UVOffset", uvOffset);
}

Material::~Material() {
}

void Material::setBlendMode(BlendMode mode) {
    blendMode = mode;
    depthWrite = (mode == BlendMode::Opaque);
    bumpRevision();
}

void Material::setOpacity(float o) {
    if (o < 0.0f) o = 0.0f;
    else if (o > 1.0f) o = 1.0f;
    opacity = o;
    setFloat("u_Opacity", opacity);
}

void Material::setAlphaCutoff(float cutoff) {
    if (cutoff < 0.0f) cutoff = 0.0f;
    else if (cutoff > 1.0f) cutoff = 1.0f;
    alphaCutoff = cutoff;
    setFloat("u_AlphaCutoff", alphaCutoff);
}

glm::vec3 Material::getColorLinear() const
{
    return glm::vec3(
        std::pow(color.r, 2.2f),
        std::pow(color.g, 2.2f),
        std::pow(color.b, 2.2f)
    );
}

void Material::setShader(std::shared_ptr<Shader> materialShader) {
    shader = materialShader;
    if (!materialShader || materialShader == Shader::getLightingShader()) {
        shaderVertexPathLinux = "";
        shaderFragmentPathLinux = "";
        shaderVertexPathVita = "";
        shaderFragmentPathVita = "";
        shaderVertexPathVulkan = "";
        shaderFragmentPathVulkan = "";
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
    } else if (platform == "vulkan" || platform == "Vulkan" || platform == "VULKAN") {
        shaderVertexPathVulkan = vertexPath;
        shaderFragmentPathVulkan = fragmentPath;
        return;
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
    } else if (platform == "vulkan" || platform == "Vulkan" || platform == "VULKAN") {
        return shaderVertexPathVulkan;
    } else {
        return shaderVertexPathLinux;
    }
}

std::string Material::getShaderFragmentPathForPlatform(const std::string& platform) const {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        return shaderFragmentPathVita;
    } else if (platform == "vulkan" || platform == "Vulkan" || platform == "VULKAN") {
        return shaderFragmentPathVulkan;
    } else {
        return shaderFragmentPathLinux;
    }
}

void Material::setShaderVertexPathForPlatform(const std::string& platform, const std::string& path) {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        shaderVertexPathVita = path;
    } else if (platform == "vulkan" || platform == "Vulkan" || platform == "VULKAN") {
        shaderVertexPathVulkan = path;
    } else {
        shaderVertexPathLinux = path;
    }
}

void Material::setShaderFragmentPathForPlatform(const std::string& platform, const std::string& path) {
    if (platform == "vita" || platform == "Vita" || platform == "VITA") {
        shaderFragmentPathVita = path;
    } else if (platform == "vulkan" || platform == "Vulkan" || platform == "VULKAN") {
        shaderFragmentPathVulkan = path;
    } else {
        shaderFragmentPathLinux = path;
    }
}

bool Material::isUsingCustomShader() const {
#ifdef VITA_BUILD
    return !shaderVertexPathVita.empty() && !shaderFragmentPathVita.empty();
#elif defined(ENABLE_VULKAN)
    if (!shaderVertexPathVulkan.empty() && !shaderFragmentPathVulkan.empty()) {
        return true;
    }
    return !shaderVertexPathLinux.empty() && !shaderFragmentPathLinux.empty();
#else
    return !shaderVertexPathLinux.empty() && !shaderFragmentPathLinux.empty();
#endif
}

#ifdef ENABLE_VULKAN
bool Material::isBeamMaterial() const {
    if (!shaderVertexPathVulkan.empty()) {
        const auto slash = shaderVertexPathVulkan.find_last_of("/\\");
        std::string file = slash != std::string::npos ? shaderVertexPathVulkan.substr(slash + 1) : shaderVertexPathVulkan;
        const auto dot = file.find_last_of('.');
        if ((dot != std::string::npos ? file.substr(0, dot) : file) == "beam") {
            return true;
        }
    }
    if (!isUsingCustomShader()) {
        return false;
    }
    const std::string path = getShaderVertexPath();
    const auto slash = path.find_last_of("/\\");
    std::string file = slash != std::string::npos ? path.substr(slash + 1) : path;
    const auto dot = file.find_last_of('.');
    return (dot != std::string::npos ? file.substr(0, dot) : file) == "beam";
}

VulkanShaderPipelineKind Material::getVulkanShaderPipelineKind() const {
    if (!isUsingCustomShader()) {
        return VulkanShaderPipelineKind::DefaultLit;
    }
    if (isBeamMaterial()) {
        return VulkanShaderPipelineKind::Beam;
    }
    return VulkanShaderPipelineKind::Custom;
}

std::string Material::getVulkanVertexSpvPath() const {
    if (!shaderVertexPathVulkan.empty()) {
        if (shaderVertexPathVulkan.size() >= 4 &&
            shaderVertexPathVulkan.compare(shaderVertexPathVulkan.size() - 4, 4, ".spv") == 0) {
            return shaderVertexPathVulkan;
        }
        return shaderVertexPathVulkan + ".spv";
    }
    if (!isUsingCustomShader()) {
        return "assets/vulkan/default_lit.vert.spv";
    }

    const std::string path = getShaderVertexPath();
    const auto slash = path.find_last_of("/\\");
    std::string file = slash != std::string::npos ? path.substr(slash + 1) : path;
    const auto dot = file.find_last_of('.');
    const std::string base = dot != std::string::npos ? file.substr(0, dot) : file;
    return "assets/vulkan/" + base + ".vert.spv";
}

std::string Material::getVulkanFragmentSpvPath() const {
    if (!shaderFragmentPathVulkan.empty()) {
        if (shaderFragmentPathVulkan.size() >= 4 &&
            shaderFragmentPathVulkan.compare(shaderFragmentPathVulkan.size() - 4, 4, ".spv") == 0) {
            return shaderFragmentPathVulkan;
        }
        return shaderFragmentPathVulkan + ".spv";
    }
    if (!isUsingCustomShader()) {
        return "assets/vulkan/default_lit.frag.spv";
    }

    const std::string path = getShaderFragmentPath();
    const auto slash = path.find_last_of("/\\");
    std::string file = slash != std::string::npos ? path.substr(slash + 1) : path;
    const auto dot = file.find_last_of('.');
    const std::string base = dot != std::string::npos ? file.substr(0, dot) : file;
    return "assets/vulkan/" + base + ".frag.spv";
}

std::string Material::getVulkanShaderPipelineKey() const {
    return getVulkanVertexSpvPath() + "|" + getVulkanFragmentSpvPath();
}

std::string Material::getCustomTexturePathByUniformName(const std::string& uniformName) const {
    for (const auto& entry : customTextureUniforms) {
        if (entry.first == uniformName) {
            return entry.second;
        }
    }
    return "";
}

bool Material::hasCustomTextureUniform(const std::string& uniformName) const {
    return !getCustomTexturePathByUniformName(uniformName).empty();
}
#endif

void Material::useDefaultLitShader() {
    shader = Shader::getLightingShader();
    shaderVertexPathLinux = "";
    shaderFragmentPathLinux = "";
    shaderVertexPathVita = "";
    shaderFragmentPathVita = "";
    shaderVertexPathVulkan = "";
    shaderFragmentPathVulkan = "";
}

std::shared_ptr<Material> Material::clone() const {
    auto copy = std::make_shared<Material>(*this);
    copy->sharedInstance = false;
    return copy;
}

void Material::rebindTextureFromPath(
    const std::string& path,
    void (Material::*setTextureFn)(std::shared_ptr<Texture>, const std::string&))
{
    auto texture = TextureManager::getInstance().getTexture(path);
    if (!texture) {
        std::cerr << "Material: override texture not found: " << path << std::endl;
        return;
    }
    (this->*setTextureFn)(texture, path);
}

void Material::applyOverride(const MaterialOverride& overrides) {
    if (overrides.overrideBaseColor) {
        setColor(overrides.baseColor);
    }
    if (overrides.overrideMetallic) {
        setMetallic(overrides.metallic);
    }
    if (overrides.overrideRoughness) {
        setRoughness(overrides.roughness);
    }
    if (overrides.overrideReflectionStrength) {
        setReflectionStrength(overrides.reflectionStrength);
    }
    if (overrides.overrideOpacity) {
        setOpacity(overrides.opacity);
    }
    if (overrides.overrideAlphaCutoff) {
        setAlphaCutoff(overrides.alphaCutoff);
    }
    if (overrides.overrideBlendMode) {
        // setBlendMode also resets depthWrite, so it has to run before it.
        setBlendMode(overrides.blendMode);
    }
    if (overrides.overrideDoubleSided) {
        setDoubleSided(overrides.doubleSided);
    }
    if (overrides.overrideUVTransform) {
        setUVScale(overrides.uvScale);
        setUVOffset(overrides.uvOffset);
    }

    if (!overrides.diffuseTexturePath.empty()) {
        rebindTextureFromPath(overrides.diffuseTexturePath, &Material::setDiffuseTexture);
    }
    if (!overrides.normalTexturePath.empty()) {
        rebindTextureFromPath(overrides.normalTexturePath, &Material::setNormalTexture);
    }
    if (!overrides.armTexturePath.empty()) {
        rebindTextureFromPath(overrides.armTexturePath, &Material::setARMTexture);
    }

    if (!overrides.shaderVertexPathLinux.empty() && !overrides.shaderFragmentPathLinux.empty()) {
        setShaderFromPathsForPlatform(overrides.shaderVertexPathLinux, overrides.shaderFragmentPathLinux, "linux");
    }
    if (!overrides.shaderVertexPathVita.empty() && !overrides.shaderFragmentPathVita.empty()) {
        setShaderFromPathsForPlatform(overrides.shaderVertexPathVita, overrides.shaderFragmentPathVita, "vita");
    }
    if (!overrides.shaderVertexPathVulkan.empty() && !overrides.shaderFragmentPathVulkan.empty()) {
        setShaderFromPathsForPlatform(overrides.shaderVertexPathVulkan, overrides.shaderFragmentPathVulkan, "vulkan");
    }

    if (shader && shader->isValid()) {
        ShaderManager::getInstance().registerShaderType(shader, ShaderType::Lit);
    }

    bumpRevision();
}

void Material::addCustomTextureUniform(const std::string& uniformName, const std::string& texturePath) {
    if (uniformName.empty() || texturePath.empty()) return;
    auto& textureManager = TextureManager::getInstance();
    auto texture = textureManager.getTexture(texturePath);
    if (!texture) return;
    for (auto& p : customTextureUniforms) {
        if (p.first == uniformName) {
            p.second = texturePath;
            setTexture(uniformName, texture);
            return;
        }
    }
    customTextureUniforms.push_back({ uniformName, texturePath });
    setTexture(uniformName, texture);
}

void Material::removeCustomTextureUniform(const std::string& uniformName) {
    customTextureUniforms.erase(
        std::remove_if(customTextureUniforms.begin(), customTextureUniforms.end(),
            [&uniformName](const CustomTextureUniform& p) { return p.first == uniformName; }),
        customTextureUniforms.end());
    textureProperties.erase(uniformName);
}

void Material::setCustomTextureUniformPath(const std::string& uniformName, const std::string& texturePath) {
    if (texturePath.empty()) return;
    auto& textureManager = TextureManager::getInstance();
    auto texture = textureManager.getTexture(texturePath);
    if (!texture) return;
    for (auto& p : customTextureUniforms) {
        if (p.first == uniformName) {
            p.second = texturePath;
            setTexture(uniformName, texture);
            return;
        }
    }
    customTextureUniforms.push_back({ uniformName, texturePath });
    setTexture(uniformName, texture);
}

void Material::setFloat(const std::string& name, float value) {
    floatProperties[name] = value;
    bumpRevision();
}

void Material::setInt(const std::string& name, int value) {
    intProperties[name] = value;
    bumpRevision();
}

void Material::setBool(const std::string& name, bool value) {
    boolProperties[name] = value;
    bumpRevision();
}

void Material::setVec2(const std::string& name, const glm::vec2& value) {
    vec2Properties[name] = value;
    bumpRevision();
}

void Material::setVec3(const std::string& name, const glm::vec3& value) {
    vec3Properties[name] = value;
    bumpRevision();
}

void Material::setVec4(const std::string& name, const glm::vec4& value) {
    vec4Properties[name] = value;
    bumpRevision();
}

void Material::setMat3(const std::string& name, const glm::mat3& value) {
    mat3Properties[name] = value;
    bumpRevision();
}

void Material::setMat4(const std::string& name, const glm::mat4& value) {
    mat4Properties[name] = value;
    bumpRevision();
}

void Material::setTexture(const std::string& name, std::shared_ptr<Texture> texture) {
    textureProperties[name] = texture;
    bumpRevision();
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

void Material::setEnvironmentTexture(std::shared_ptr<Texture> texture, const std::string& path) {
    environmentTexture = texture;
    if (texture == nullptr) {
        environmentTexturePath = "";
    } else if (!path.empty()) {
        environmentTexturePath = path;
    }
    setTexture("u_EnvironmentTexture", texture);
    setBool("u_HasEnvironmentTexture", texture != nullptr);
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
    
    glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
    glDepthFunc(depthWrite ? GL_LESS : GL_LEQUAL);
    
    auto& shaderManager = ShaderManager::getInstance();

    bool isLit = (shader == Shader::getLightingShader()) || (shader == Shader::getLightingInstancedShader()) || shaderManager.isLitShader(shader);

    if (isLit) {
        setupLightingUniforms();
    }
}

void Material::applyWithShader(const std::shared_ptr<Shader>& shaderOverride) const {
    if (!shaderOverride || !shaderOverride->isValid()) {
        apply();
        return;
    }

    Material* self = const_cast<Material*>(this);
    std::shared_ptr<Shader> previousShader = self->shader;
    self->shader = shaderOverride;
    apply();
    self->shader = previousShader;
}

bool Material::drawShaderAssignPopup(
    const char* popupId,
    std::string& outVertexPath,
    std::string& outFragmentPath,
    std::string& outPlatform)
{
#ifdef EDITOR_BUILD
    ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
    if (!ImGui::BeginPopup(popupId)) {
        return false;
    }

    static char vertexPathBuffer[512] = "";
    static char fragmentPathBuffer[512] = "";
    static int platformSelection = 0;
    static std::map<std::string, std::vector<std::string>> shaderListCache;

    if (ImGui::IsWindowAppearing()) {
        shaderListCache.clear();
    }

    const char* platforms[] = { "Linux (GLSL)", "Vita (CG)", "Vulkan (GLSL)" };
    ImGui::Combo("Platform", &platformSelection, platforms, 3);

    ImGui::Separator();

    if (platformSelection == 2) {
        ImGui::TextWrapped("Select Vulkan GLSL sources under assets/vulkan/. Assign compiles matching .spv with glslc.");
    }

    std::string platformDir = (platformSelection == 0) ? "assets/linux_shaders"
                           : (platformSelection == 1) ? "assets/shaders"
                                                      : "assets/vulkan";
    auto cached = shaderListCache.find(platformDir);
    if (cached == shaderListCache.end()) {
        cached = shaderListCache.emplace(platformDir, ShaderManager::discoverShaders(platformDir, ".vert")).first;
    }
    const auto& vertexShaders = cached->second;

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
        const std::string picked = FileDialog::openFileDialog("Select Vertex Shader", "*.vert");
        if (FileDialog::isValidResult(picked)) {
            std::string importError;
            const std::string imported = ProjectAssets::importIntoProject(picked, importError);
            if (imported.empty()) {
                std::cerr << importError << std::endl;
            } else {
                strncpy(vertexPathBuffer, imported.c_str(), sizeof(vertexPathBuffer) - 1);
                vertexPathBuffer[sizeof(vertexPathBuffer) - 1] = '\0';
            }
        }
#endif
    }
#ifdef LINUX_BUILD
    if (ImGui::BeginDragDropTarget()) {
        const std::string dropped = ProjectAssets::acceptDrop(ProjectAssets::Kind::Shader);
        if (!dropped.empty()) {
            strncpy(vertexPathBuffer, dropped.c_str(), sizeof(vertexPathBuffer) - 1);
            vertexPathBuffer[sizeof(vertexPathBuffer) - 1] = '\0';
        }
        ImGui::EndDragDropTarget();
    }
#endif

    ImGui::SameLine();

    if (ImGui::Button("Browse Fragment...")) {
#ifdef LINUX_BUILD
        const std::string picked = FileDialog::openFileDialog("Select Fragment Shader", "*.frag");
        if (FileDialog::isValidResult(picked)) {
            std::string importError;
            const std::string imported = ProjectAssets::importIntoProject(picked, importError);
            if (imported.empty()) {
                std::cerr << importError << std::endl;
            } else {
                strncpy(fragmentPathBuffer, imported.c_str(), sizeof(fragmentPathBuffer) - 1);
                fragmentPathBuffer[sizeof(fragmentPathBuffer) - 1] = '\0';
            }
        }
#endif
    }
#ifdef LINUX_BUILD
    if (ImGui::BeginDragDropTarget()) {
        const std::string dropped = ProjectAssets::acceptDrop(ProjectAssets::Kind::Shader);
        if (!dropped.empty()) {
            strncpy(fragmentPathBuffer, dropped.c_str(), sizeof(fragmentPathBuffer) - 1);
            fragmentPathBuffer[sizeof(fragmentPathBuffer) - 1] = '\0';
        }
        ImGui::EndDragDropTarget();
    }
#endif

    bool assigned = false;

    if (ImGui::Button("Assign")) {
        if (strlen(vertexPathBuffer) > 0 && strlen(fragmentPathBuffer) > 0) {
            const std::string platform = (platformSelection == 0) ? "linux"
                                       : (platformSelection == 1) ? "vita"
                                                                  : "vulkan";

            bool ready = true;
            if (platform == "vulkan") {
                std::string compileError;
                if (!ShaderManager::compileVulkanGlslToSpv(vertexPathBuffer, &compileError)) {
                    std::cerr << "Vulkan vertex SPIR-V compile failed: " << compileError << std::endl;
                    ready = false;
                } else if (!ShaderManager::compileVulkanGlslToSpv(fragmentPathBuffer, &compileError)) {
                    std::cerr << "Vulkan fragment SPIR-V compile failed: " << compileError << std::endl;
                    ready = false;
                }
            }

            if (ready) {
                outVertexPath = vertexPathBuffer;
                outFragmentPath = fragmentPathBuffer;
                outPlatform = platform;
                assigned = true;
                ImGui::CloseCurrentPopup();
                vertexPathBuffer[0] = '\0';
                fragmentPathBuffer[0] = '\0';
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
        vertexPathBuffer[0] = '\0';
        fragmentPathBuffer[0] = '\0';
    }

    ImGui::EndPopup();
    return assigned;
#else
    (void)popupId;
    (void)outVertexPath;
    (void)outFragmentPath;
    (void)outPlatform;
    return false;
#endif
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

        if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 1.0f)) {
            setOpacity(opacity);
        }
        
        const char* blendModeNames[] = { "Opaque", "Alpha", "Additive" };
        int currentBlend = static_cast<int>(blendMode);
        if (ImGui::Combo("Blend Mode", &currentBlend, blendModeNames, 3)) {
            setBlendMode(static_cast<BlendMode>(currentBlend));
        }

        ImGui::Checkbox("Depth Write", &depthWrite);
        
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
        
        ImGui::Text("UV Scale");
        if (ImGui::DragFloat2("##UVScale", &uvScale.x, 0.01f)) {
            setUVScale(uvScale);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##UVScale")) {
            setUVScale(glm::vec2(1.0f, 1.0f));
        }
        
        ImGui::Text("UV Offset");
        if (ImGui::DragFloat2("##UVOffset", &uvOffset.x, 0.01f)) {
            setUVOffset(uvOffset);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##UVOffset")) {
            setUVOffset(glm::vec2(0.0f, 0.0f));
        }
        
        ImGui::Separator();
        ImGui::Text("Shader");
        
        bool isDefaultLit = (shader == Shader::getLightingShader());
        const bool hasLinuxCustom = !shaderVertexPathLinux.empty() && !shaderFragmentPathLinux.empty();
        const bool hasVitaCustom = !shaderVertexPathVita.empty() && !shaderFragmentPathVita.empty();
        const bool hasVulkanCustom = !shaderVertexPathVulkan.empty() && !shaderFragmentPathVulkan.empty();
        const bool hasAnyCustomPaths = hasLinuxCustom || hasVitaCustom || hasVulkanCustom;
        
        if (isDefaultLit && !hasAnyCustomPaths) {
            ImGui::Text("Type: Default Lit Shader");
#ifdef LINUX_BUILD
            ImGui::Text("Platform: Linux (GLSL)");
#else
            ImGui::Text("Platform: Vita (CG)");
#endif
        } else if (hasAnyCustomPaths) {
            ImGui::Text("Type: Custom Shader");
            
            if (hasLinuxCustom) {
                ImGui::Text("Linux (GLSL):");
                ImGui::Text("  Vertex: %s", shaderVertexPathLinux.c_str());
                ImGui::Text("  Fragment: %s", shaderFragmentPathLinux.c_str());
            }
            
            if (hasVitaCustom) {
                ImGui::Text("Vita (CG):");
                ImGui::Text("  Vertex: %s", shaderVertexPathVita.c_str());
                ImGui::Text("  Fragment: %s", shaderFragmentPathVita.c_str());
            }

            if (hasVulkanCustom) {
                ImGui::Text("Vulkan (GLSL/SPIR-V):");
                ImGui::Text("  Vertex: %s", shaderVertexPathVulkan.c_str());
                ImGui::Text("  Fragment: %s", shaderFragmentPathVulkan.c_str());
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
        
        {
            std::string assignedVertexPath;
            std::string assignedFragmentPath;
            std::string assignedPlatform;
            if (drawShaderAssignPopup("AssignShaderPopup", assignedVertexPath, assignedFragmentPath, assignedPlatform)) {
                setShaderFromPathsForPlatform(assignedVertexPath, assignedFragmentPath, assignedPlatform);
                if (shader && shader->isValid()) {
                    ShaderManager::getInstance().registerShaderType(shader, ShaderType::Lit);
                }
            }
        }

        if (isUsingCustomShader() || hasAnyCustomPaths) {
            ImGui::Separator();
            ImGui::Text("Custom texture uniforms");
            auto& textureManager = TextureManager::getInstance();
            auto availableTextures = textureManager.getAvailableTextures();
            std::vector<std::string> toRemoveCustomUniforms;
            for (const auto& entry : getCustomTextureUniforms()) {
                const std::string& uniformName = entry.first;
                std::string currentPath = entry.second;
                ImGui::PushID(uniformName.c_str());
                ImGui::Text("%s", uniformName.c_str());
                std::string comboLabel = currentPath.empty() ? "None" : currentPath;
                if (comboLabel.length() > 60) {
                    size_t start = comboLabel.find("assets/");
                    if (start != std::string::npos) comboLabel = comboLabel.substr(start);
                    if (comboLabel.length() > 60) comboLabel = "..." + comboLabel.substr(comboLabel.length() - 57);
                }
                if (ImGui::BeginCombo("Texture", comboLabel.c_str())) {
                    if (ImGui::Selectable("None", currentPath.empty())) {
                        toRemoveCustomUniforms.push_back(uniformName);
                    }
                    for (const auto& texturePath : availableTextures) {
                        bool selected = (texturePath == currentPath);
                        if (ImGui::Selectable(texturePath.c_str(), selected)) {
                            setCustomTextureUniformPath(uniformName, texturePath);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    toRemoveCustomUniforms.push_back(uniformName);
                }
                ImGui::PopID();
            }
            for (const auto& name : toRemoveCustomUniforms) {
                removeCustomTextureUniform(name);
            }
            if (ImGui::Button("Add texture uniform")) {
                textureManager.discoverAllTextures("assets/textures");
                ImGui::OpenPopup("AddTextureUniformPopup");
            }
            if (ImGui::BeginPopup("AddTextureUniformPopup")) {
                static char customUniformNameBuffer[128] = "";
                static int customTextureSelectedIdx = -1;
                ImGui::InputText("Uniform name (e.g. u_NoiseTexture)", customUniformNameBuffer, sizeof(customUniformNameBuffer));
                ImGui::Text("Select texture from assets/textures:");
                if (ImGui::BeginChild("CustomTextureList", ImVec2(0, 120), true)) {
                    for (size_t i = 0; i < availableTextures.size(); ++i) {
                        const auto& texturePath = availableTextures[i];
                        bool selected = (customTextureSelectedIdx >= 0 && static_cast<size_t>(customTextureSelectedIdx) == i);
                        if (ImGui::Selectable(texturePath.c_str(), selected)) {
                            customTextureSelectedIdx = static_cast<int>(i);
                        }
                    }
                    ImGui::EndChild();
                }
                if (ImGui::Button("Add")) {
                    if (strlen(customUniformNameBuffer) > 0 && customTextureSelectedIdx >= 0 &&
                        static_cast<size_t>(customTextureSelectedIdx) < availableTextures.size()) {
                        addCustomTextureUniform(customUniformNameBuffer, availableTextures[static_cast<size_t>(customTextureSelectedIdx)]);
                        customUniformNameBuffer[0] = '\0';
                        customTextureSelectedIdx = -1;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    customUniformNameBuffer[0] = '\0';
                    customTextureSelectedIdx = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
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
        defaultMaterial->setColor(glm::vec3(1.0f, 1.0f, 1.0f));
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
    shader->setFloat("u_Opacity", opacity);
    shader->setFloat("u_AlphaCutoff", alphaCutoff);
    
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
    
    // Fixed units for the lit shader's samplers. GLSL sampler uniforms default
    // to unit 0, if samplerCube (u_EnvironmentMap) and a sampler2D both point
    // at unit 0, the draw fails with GL_INVALID_OPERATION and the mesh vanishes.
    // This shows up in the editor on scenes with a skybox (cubemap exists) when
    // ModelRenderer draws without injecting the env map, MeshRenderer was fine
    // because it always bound the cubemap onto a later unit
    constexpr int kDiffuseUnit = 0;
    constexpr int kNormalUnit = 1;
    constexpr int kArmUnit = 2;
    constexpr int kEnvironmentUnit = 3;

    shader->setInt("u_DiffuseTexture", kDiffuseUnit);
    shader->setInt("u_NormalTexture", kNormalUnit);
    shader->setInt("u_ARMTexture", kArmUnit);
    shader->setInt("u_EnvironmentMap", kEnvironmentUnit);
    shader->setInt("u_ShadowMap", kShadowMapTextureUnit);
    
    if (diffuseTexture) {
        diffuseTexture->bind(kDiffuseUnit);
        shader->setBool("u_HasDiffuseTexture", true);
    } else {
        shader->setBool("u_HasDiffuseTexture", false);
    }
    
    if (normalTexture) {
        normalTexture->bind(kNormalUnit);
        shader->setBool("u_HasNormalTexture", true);
    } else {
        shader->setBool("u_HasNormalTexture", false);
    }
    
    if (armTexture) {
        armTexture->bind(kArmUnit);
        shader->setBool("u_HasARMTexture", true);
    } else {
        shader->setBool("u_HasARMTexture", false);
    }

    auto envIt = textureProperties.find("u_EnvironmentMap");
    if (envIt != textureProperties.end() && envIt->second && envIt->second->isCubemap()) {
        envIt->second->bindCubemap(kEnvironmentUnit);
    }
    
    // (0-3 textures, 4 shadow atlas).
    int textureUnit = kShadowMapTextureUnit + 1;

    for (const auto& prop : textureProperties) {
        if (!prop.second) continue;
        if (prop.first == "u_EnvironmentMap") continue; // already on unit 3
        if ((prop.first == "skybox") && prop.second->isCubemap()) {
            prop.second->bindCubemap(textureUnit);
        } else {
            prop.second->bind(textureUnit);
        }
        shader->setInt(prop.first, textureUnit);
        textureUnit++;
    }
}

void Material::setupLightingUniforms() const {
    if (!shader) return;
    
    if (floatProperties.find("Kd") == floatProperties.end()) {
        shader->setFloat("Kd", 1.0f);
    }
    
    auto cameraPosIt = vec3Properties.find("u_CameraPos");
    if (cameraPosIt != vec3Properties.end()) {
        shader->setVec3("u_CameraPos", cameraPosIt->second);
    } else {
        shader->setVec3("u_CameraPos", glm::vec3(0.0f, 0.0f, 5.0f));
    }

    auto& lightingManager = LightingManager::getInstance();

    const uint32_t passStamp = lightingManager.getPassStamp();
    if (shader->lightingPassStamp == passStamp) {
        return;
    }
    shader->lightingPassStamp = passStamp;

    const auto& lightDataArray = lightingManager.getLightDataArray();

    shader->setInt("u_NumLights", static_cast<int>(lightingManager.getActiveLightCount()));

    struct LightUniformNames {
        std::string position, direction, color, params, attenuation;
    };
    static const std::vector<LightUniformNames> uniformNames = [] {
        std::vector<LightUniformNames> names;
        names.reserve(LightingManager::MAX_LIGHTS);
        for (size_t i = 0; i < LightingManager::MAX_LIGHTS; ++i) {
            const std::string base = "u_Lights[" + std::to_string(i) + "]";
            names.push_back({base + ".position", base + ".direction", base + ".color",
                             base + ".params", base + ".attenuation"});
        }
        return names;
    }();

    const size_t lightCount = std::min(lightDataArray.size(), uniformNames.size());
    for (size_t i = 0; i < lightCount; ++i) {
        const auto& lightData = lightDataArray[i];
        const auto& names = uniformNames[i];

        shader->setVec4(names.position, lightData.position);
        shader->setVec4(names.direction, lightData.direction);
        shader->setVec4(names.color, lightData.color);
        shader->setVec4(names.params, lightData.params);
        shader->setVec4(names.attenuation, lightData.attenuation);
    }

    setupShadowUniforms();
}

void Material::setupShadowUniforms() const {
    if (!shader->hasUniform("u_NumShadowViews")) {
        return;
    }

    const auto& shadowManager = ShadowManager::getInstance();
    const std::vector<glm::mat4>& shadowMatrices = shadowManager.getViewMatrices();

    if (shadowMatrices.empty() || !LightingManager::getInstance().isShadowMapBound()) {
        shader->setInt("u_NumShadowViews", 0);
        return;
    }

    shader->setInt("u_NumShadowViews", static_cast<int>(shadowMatrices.size()));
    shader->setVec4("u_ShadowParams", shadowManager.getShaderParams());
    shader->setMat4Array("u_ShadowMatrices", shadowMatrices.data(), shadowMatrices.size());
}

} // namespace GameEngine
