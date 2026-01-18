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

struct RenderCommand {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    glm::mat4 modelMatrix;
    glm::mat3 normalMatrix;
    std::vector<glm::mat4> boneTransforms;  // For animated/skinned meshes
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
    
    void updateLightingUniforms();
    glm::vec3 extractCameraPosition(const glm::mat4& viewMatrix);
    
    struct RenderStats {
        int drawCalls;
        int triangles;
        int vertices;
        void reset() { drawCalls = triangles = vertices = 0; }
    };
    
    const RenderStats& getStats() const { return stats; }
    void resetStats() { stats.reset(); }
    
private:
    CameraComponent* activeCamera;
    glm::ivec4 viewport;
    glm::vec3 clearColor;
    
    std::vector<RenderCommand> renderQueue;
    RenderStats stats;
    
    bool wireframeEnabled;
    bool depthTestEnabled;
    bool cullFaceEnabled;
    
    void processRenderQueue();
    void setupCamera();
    void applyMaterial(const Material& material);
};

} // namespace GameEngine

#endif // RENDERER_H
