#ifndef SKYBOX_COMPONENT_H
#define SKYBOX_COMPONENT_H

#include "Components/Component.h"
#include <string>
#include <memory>
#include <vector>

namespace GameEngine {

class Texture;
class Mesh;
class Material;
class Shader;

class SkyboxComponent : public Component {
public:
    SkyboxComponent();
    virtual ~SkyboxComponent();
    
    COMPONENT_TYPE(SkyboxComponent)
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void render(Renderer& renderer) override;
    virtual void destroy() override;
    
    bool isActive() const { return isActiveSkybox; }
    void setActive(bool active);
    
    bool setTextures(const std::vector<std::string>& texturePaths);
    
    void setRightTexture(const std::string& path);
    void setLeftTexture(const std::string& path);
    void setTopTexture(const std::string& path);
    void setBottomTexture(const std::string& path);
    void setFrontTexture(const std::string& path);
    void setBackTexture(const std::string& path);
    
    const std::vector<std::string>& getTexturePaths() const { return texturePaths; }
    
    std::shared_ptr<Texture> getCubemapTexture() const { return cubemapTexture; }
    
    std::shared_ptr<Mesh> getSkyboxMesh() const { return skyboxMesh; }
    std::shared_ptr<Material> getSkyboxMaterial() const { return skyboxMaterial; }
    
    virtual void drawInspector() override;
    
private:
    bool isActiveSkybox;
    std::vector<std::string> texturePaths;
    
    std::shared_ptr<Texture> cubemapTexture;
    std::shared_ptr<Mesh> skyboxMesh;
    std::shared_ptr<Material> skyboxMaterial;
    std::shared_ptr<Shader> skyboxShader;
    
    bool loadCubemap();
    void createSkyboxMesh();
    void createSkyboxShader();
    void createSkyboxMaterial();
    
    static void deactivateAllSkyboxes(SceneNode* root, SkyboxComponent* exclude);
};

} // namespace GameEngine

#endif // SKYBOX_COMPONENT_H
