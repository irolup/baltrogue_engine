#ifndef RENDERER_H
#define RENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include "Platform.h"

namespace GameEngine {

class Scene;
class SceneNode;
class Mesh;
class Material;
class CameraComponent;
class SkyboxComponent;

struct RenderCommand {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    glm::mat4 modelMatrix;
    glm::mat3 normalMatrix;
    std::vector<glm::mat4> boneTransforms;
    bool disableCulling = false;
};

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    bool initialize();
    void shutdown();
    
    void beginFrame();
    void endFrame();
    void present();
    
    void renderScene(Scene& scene);
    void renderNode(SceneNode& node, const glm::mat4& parentTransform = glm::mat4(1.0f));
    
    void renderMesh(const Mesh& mesh, const Material& material, const glm::mat4& modelMatrix);
    void submitRenderCommand(const RenderCommand& command);
    
    void setActiveCamera(CameraComponent* camera);
    CameraComponent* getActiveCamera() const { return activeCamera; }
    
    void setViewport(int x, int y, int width, int height);
    glm::ivec4 getViewport() const { return viewport; }
    
    void setClearColor(const glm::vec3& color);
    void setClearColor(float r, float g, float b);
    void clear();
    
    void setWireframe(bool enabled);
    void setDepthTest(bool enabled);
    void setCullFace(bool enabled);
    void setFrustumCulling(bool enabled) { frustumCullingEnabled = enabled; }
    bool isFrustumCullingEnabled() const { return frustumCullingEnabled; }
    
    void updateLightingUniforms();
    glm::vec3 extractCameraPosition(const glm::mat4& viewMatrix);
    
    struct RenderStats {
        int drawCalls;
        int triangles;
        int vertices;
        int culledObjects;
        int totalObjectsTested;
        void reset() { drawCalls = triangles = vertices = culledObjects = totalObjectsTested = 0; }
    };
    
    const RenderStats& getStats() const { return stats; }
    void resetStats() { stats.reset(); }
    
private:
    CameraComponent* activeCamera;
    Scene* currentScene;
    glm::ivec4 viewport;
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
    
    struct FrustumPlane {
        glm::vec3 normal;
        float distance;
    };
    std::vector<FrustumPlane> frustumPlanes;
    
    void processRenderQueue();
    void setupCamera();
    void applyMaterial(const Material& material);
    void updateFrustum();
    void renderSkybox(Scene& scene);
    bool isMeshInFrustum(const Mesh& mesh, const glm::mat4& modelMatrix) const;
    bool isAABBInFrustum(const glm::vec3& min, const glm::vec3& max, const glm::mat4& transform) const;
};

} // namespace GameEngine

#endif // RENDERER_H
