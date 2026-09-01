#pragma once

#include "Rendering/IRenderer.h"
#include "Rendering/RenderTypes.h"
#include "Rendering/Frustum.h"
#include "Rendering/InstanceBatcher.h"
#include "Rendering/ShadowAtlasGL.h"
#include "Platform.h"
#include <glm/glm.hpp>
#include <vector>

namespace GameEngine {

class Scene;
class SceneNode;
class Mesh;
class Material;
class CameraComponent;
class SkyboxComponent;

class OpenGLRenderer : public IRenderer {

public:

    OpenGLRenderer();

    bool initialize() override;
    void shutdown() override;
    
    void beginFrame() override;
    void syncViewportToFramebuffer() override;
    void endFrame() override;
    void present() override;
    
    void renderScene(Scene& scene) override;
    void renderNode(SceneNode& node, const glm::mat4& parentTransform = glm::mat4(1.0f)) override;
    void renderFromCamera(Scene& scene, CameraComponent* cam, const glm::vec4& vpNorm) override;
    
    void renderMesh(const Mesh& mesh, const Material& material, const glm::mat4& modelMatrix) override;
    void submitRenderCommand(const RenderCommand& command) override;
    
    void setActiveCamera(CameraComponent* camera) override;
    CameraComponent* getActiveCamera() const override { return activeCamera; }
    
    void setViewport(int x, int y, int width, int height) override;
    glm::ivec4 getViewport() const override { return viewport; }
    
    void setClearColor(const glm::vec3& color) override;
    void setClearColor(float r, float g, float b) override;
    void clear() override;
    
    void setWireframe(bool enabled) override;
    void setDepthTest(bool enabled) override;
    void setCullFace(bool enabled) override;
    void setFrustumCulling(bool enabled) override { frustumCullingEnabled = enabled; }
    bool isFrustumCullingEnabled() const override { return frustumCullingEnabled; }

    void setInstancingEnabled(bool enabled) { instancingEnabled = enabled; }
    bool isInstancingEnabled() const { return instancingEnabled; }
    
    void updateLightingUniforms() override;
    glm::vec3 extractCameraPosition(const glm::mat4& viewMatrix) override;
    
    const RenderStats& getStats() const override { return stats; }
    void resetStats() override { stats.reset(); }

private:
    CameraComponent* activeCamera;
    Scene* currentScene;
    glm::ivec4 viewport;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glm::vec3 clearColor;
    
    std::vector<RenderCommand> renderQueue;
    RenderStats stats;
    
    bool wireframeEnabled;
    bool depthTestEnabled;
    bool cullFaceEnabled;
    bool frustumCullingEnabled;
    
    glm::mat4 cachedViewMatrix;
    glm::mat4 cachedProjectionMatrix;
    bool matricesCached;

    Frustum cameraFrustum;
    ShadowAtlasGL shadowAtlas;
    bool shadowAtlasReady = false;

    bool instancingEnabled = true;
    bool instancingSupported = true;
    InstanceBatcher instanceBatcher;
    std::vector<uint8_t> cameraVisible;
    GLuint instanceVBO = 0;
    size_t instanceVBOCapacity = 0;

    void processRenderQueue();
    void setupCamera();
    void applyMaterial(const Material& material);

    void cullRenderQueue();
    void buildInstanceBatches();

    bool drawInstanceBatches(const glm::vec3& cameraPos);
    bool ensureInstanceBuffer(size_t bytes);

    static bool isInstanceableShader(const RenderCommand& command);

    void applyEnvironmentMap(Material& material) const;
    void updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projMatrix);
    void renderSkybox(Scene& scene);
    void renderTextNodes(SceneNode& node, const glm::mat4& parentTransform);

    // Fills the shadow atlas from the queue built by renderNode()
    void renderShadowPass();
    void bindShadowResources();

};

}