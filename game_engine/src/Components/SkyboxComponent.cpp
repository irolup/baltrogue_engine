#include "Components/SkyboxComponent.h"
#include "Scene/SceneNode.h"
#include "Scene/Scene.h"
#include "Scene/SceneManager.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureManager.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Shader.h"
#include "Rendering/Renderer.h"
#include "Core/Engine.h"
#include <iostream>
#include <cstring>

#ifdef EDITOR_BUILD
    #include "imgui.h"
#endif

namespace GameEngine {

SkyboxComponent::SkyboxComponent()
    : isActiveSkybox(false)
    , texturePaths(6)
    , cubemapTexture(nullptr)
    , skyboxMesh(nullptr)
    , skyboxMaterial(nullptr)
    , skyboxShader(nullptr)
{
}

SkyboxComponent::~SkyboxComponent() {
    destroy();
}

void SkyboxComponent::start() {
    if (texturePaths.size() == 6 && !texturePaths[0].empty()) {
        loadCubemap();
        createSkyboxMesh();
        createSkyboxShader();
        createSkyboxMaterial();
    }
}

void SkyboxComponent::update(float deltaTime) {
}

void SkyboxComponent::render(Renderer& renderer) {
    if (!isActiveSkybox || !skyboxMesh || !skyboxMaterial || !cubemapTexture) {
        return;
    }
    
}

void SkyboxComponent::destroy() {
    cubemapTexture.reset();
    skyboxMesh.reset();
    skyboxMaterial.reset();
    skyboxShader.reset();
}

void SkyboxComponent::setActive(bool active) {
    if (isActiveSkybox == active) return;
    
    isActiveSkybox = active;
    
    if (active && owner) {
        SceneNode* node = owner;
        while (node->getParent()) {
            node = node->getParent();
        }
        
        auto& engine = GetEngine();
        auto& sceneManager = engine.getSceneManager();
        auto activeScene = sceneManager.getCurrentScene();
        
        if (activeScene && owner) {
            auto sceneNode = activeScene->findNode(owner->getName());
            if (sceneNode) {
                activeScene->setActiveSkybox(sceneNode);
            }
        }
    }
}

bool SkyboxComponent::setTextures(const std::vector<std::string>& paths) {
    if (paths.size() != 6) {
        std::cerr << "SkyboxComponent::setTextures requires exactly 6 texture paths" << std::endl;
        return false;
    }
    
    texturePaths = paths;
    
    if (isActiveSkybox) {
        loadCubemap();
        createSkyboxMesh();
        createSkyboxShader();
        createSkyboxMaterial();
    }
    
    return true;
}

void SkyboxComponent::setRightTexture(const std::string& path) {
    if (texturePaths.size() < 6) texturePaths.resize(6);
    texturePaths[0] = path;
}

void SkyboxComponent::setLeftTexture(const std::string& path) {
    if (texturePaths.size() < 6) texturePaths.resize(6);
    texturePaths[1] = path;
}

void SkyboxComponent::setTopTexture(const std::string& path) {
    if (texturePaths.size() < 6) texturePaths.resize(6);
    texturePaths[2] = path;
}

void SkyboxComponent::setBottomTexture(const std::string& path) {
    if (texturePaths.size() < 6) texturePaths.resize(6);
    texturePaths[3] = path;
}

void SkyboxComponent::setFrontTexture(const std::string& path) {
    if (texturePaths.size() < 6) texturePaths.resize(6);
    texturePaths[4] = path;
}

void SkyboxComponent::setBackTexture(const std::string& path) {
    if (texturePaths.size() < 6) texturePaths.resize(6);
    texturePaths[5] = path;
}

bool SkyboxComponent::loadCubemap() {
    if (texturePaths.size() != 6) {
        std::cerr << "SkyboxComponent::loadCubemap requires 6 texture paths" << std::endl;
        return false;
    }
    
    for (const auto& path : texturePaths) {
        if (path.empty()) {
            std::cerr << "SkyboxComponent::loadCubemap - not all texture paths are set" << std::endl;
            return false;
        }
    }
    
    cubemapTexture = std::make_shared<Texture>();
    if (!cubemapTexture->createCubemap(texturePaths)) {
        std::cerr << "SkyboxComponent::loadCubemap - failed to create cubemap" << std::endl;
        cubemapTexture.reset();
        return false;
    }
    
    return true;
}

void SkyboxComponent::createSkyboxMesh() {
    std::vector<Vertex> vertices = {
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}
    };
    
    std::vector<unsigned int> indices;
    
    skyboxMesh = std::make_shared<Mesh>(vertices, indices);
    skyboxMesh->setRenderMode(GL_TRIANGLES);
    skyboxMesh->upload();
}

void SkyboxComponent::createSkyboxShader() {
    skyboxShader = std::make_shared<Shader>();
    
#ifdef LINUX_BUILD
    std::string vertPath = "assets/linux_shaders/skybox.vert";
    std::string fragPath = "assets/linux_shaders/skybox.frag";
#else
    std::string vertPath = "assets/shaders/skybox.vert";
    std::string fragPath = "assets/shaders/skybox.frag";
#endif
    
    if (!skyboxShader->loadFromFiles(vertPath, fragPath)) {
        std::cerr << "SkyboxComponent::createSkyboxShader - failed to load skybox shader" << std::endl;
        skyboxShader.reset();
    }
}

void SkyboxComponent::createSkyboxMaterial() {
    if (!skyboxShader) {
        createSkyboxShader();
    }
    
    if (!skyboxShader) {
        std::cerr << "SkyboxComponent::createSkyboxMaterial - no shader available" << std::endl;
        return;
    }
    
    skyboxMaterial = std::make_shared<Material>(skyboxShader);
    skyboxMaterial->setTexture("skybox", cubemapTexture);
}

void SkyboxComponent::drawInspector() {
#ifdef EDITOR_BUILD
    ImGui::Text("Skybox Component");
    
    bool active = isActiveSkybox;
    if (ImGui::Checkbox("Active", &active)) {
        setActive(active);
    }
    
    ImGui::Separator();
    ImGui::Text("Texture Paths:");
    
    static char rightPath[256] = "";
    static char leftPath[256] = "";
    static char topPath[256] = "";
    static char bottomPath[256] = "";
    static char frontPath[256] = "";
    static char backPath[256] = "";
    
    if (texturePaths.size() >= 6) {
        strncpy(rightPath, texturePaths[0].c_str(), sizeof(rightPath) - 1);
        strncpy(leftPath, texturePaths[1].c_str(), sizeof(leftPath) - 1);
        strncpy(topPath, texturePaths[2].c_str(), sizeof(topPath) - 1);
        strncpy(bottomPath, texturePaths[3].c_str(), sizeof(bottomPath) - 1);
        strncpy(frontPath, texturePaths[4].c_str(), sizeof(frontPath) - 1);
        strncpy(backPath, texturePaths[5].c_str(), sizeof(backPath) - 1);
    }
    
    if (ImGui::InputText("Right (+X)", rightPath, sizeof(rightPath))) {
        setRightTexture(rightPath);
    }
    if (ImGui::InputText("Left (-X)", leftPath, sizeof(leftPath))) {
        setLeftTexture(leftPath);
    }
    if (ImGui::InputText("Top (+Y)", topPath, sizeof(topPath))) {
        setTopTexture(topPath);
    }
    if (ImGui::InputText("Bottom (-Y)", bottomPath, sizeof(bottomPath))) {
        setBottomTexture(bottomPath);
    }
    if (ImGui::InputText("Front (+Z)", frontPath, sizeof(frontPath))) {
        setFrontTexture(frontPath);
    }
    if (ImGui::InputText("Back (-Z)", backPath, sizeof(backPath))) {
        setBackTexture(backPath);
    }
    
    if (ImGui::Button("Reload Cubemap")) {
        loadCubemap();
        createSkyboxMesh();
        createSkyboxShader();
        createSkyboxMaterial();
    }
    
    if (cubemapTexture) {
        ImGui::Text("Cubemap: Loaded");
    } else {
        ImGui::Text("Cubemap: Not Loaded");
    }
#endif
}

void SkyboxComponent::deactivateAllSkyboxes(SceneNode* root, SkyboxComponent* exclude) {
    if (!root) return;
    
    auto skyboxComp = root->getComponent<SkyboxComponent>();
    if (skyboxComp && skyboxComp != exclude) {
        skyboxComp->isActiveSkybox = false;
    }
    
    for (size_t i = 0; i < root->getChildCount(); ++i) {
        deactivateAllSkyboxes(root->getChild(i).get(), exclude);
    }
}

} // namespace GameEngine
