#pragma once

#include "Rendering/RenderTypes.h"
#include <glm/glm.hpp>

namespace GameEngine {

class Scene;
class SceneNode;
class Mesh;
class Material;
class CameraComponent;

class IRenderer {
    public:

    struct RenderStats {
        int drawCalls = 0;
        int triangles = 0;
        int vertices = 0;
        int culledObjects = 0;
        int totalObjectsTested = 0;
        void reset() { drawCalls = triangles = vertices = culledObjects = totalObjectsTested = 0; }
    };

    virtual ~IRenderer() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual void beginFrame() = 0;
    virtual void syncViewportToFramebuffer() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;

    virtual void renderScene(Scene& scene) = 0;
    virtual void renderNode(SceneNode& node, const glm::mat4& parentTransform = glm::mat4(1.0f)) = 0;
    virtual void renderFromCamera(Scene& scene, CameraComponent* cam, const glm::vec4& vpNorm) = 0;

    virtual void renderMesh(const Mesh& mesh, const Material& material, const glm::mat4& modelMatrix) = 0;
    virtual void submitRenderCommand(const RenderCommand& command) = 0;
    virtual void submitTextRenderCommand(const TextRenderCommand& command) {}

    virtual void setActiveCamera(CameraComponent* camera) = 0;
    virtual CameraComponent* getActiveCamera() const = 0;

    virtual void setViewport(int x, int y, int width, int height) = 0;
    virtual glm::ivec4 getViewport() const = 0;

    virtual void setClearColor(const glm::vec3& color) = 0;
    virtual void setClearColor(float r, float g, float b) = 0;
    virtual void clear() = 0;

    virtual void setWireframe(bool enabled) = 0;
    virtual void setDepthTest(bool enabled) = 0;
    virtual void setCullFace(bool enabled) = 0;
    virtual void setFrustumCulling(bool enabled) = 0;
    virtual bool isFrustumCullingEnabled() const = 0;

    virtual void updateLightingUniforms() = 0;
    virtual glm::vec3 extractCameraPosition(const glm::mat4& viewMatrix) = 0;
    
    virtual const RenderStats& getStats() const = 0;
    virtual void resetStats() = 0;

};

}