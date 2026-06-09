#pragma once

#include "Rendering/IRenderer.h"
#include "Rendering/Vulkan/VulkanDevice.h"
#include "Rendering/Vulkan/VulkanSwapChain.h"
#include "Rendering/Vulkan/VulkanResources.h"
#include "Rendering/Vulkan/VulkanPipeline.h"

namespace GameEngine {

class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() override = default;

    bool initialize() override { return true; }
    void shutdown() override {}

    void beginFrame() override;
    void syncViewportToFramebuffer() override {}
    void endFrame() override;
    void present() override {}

    void renderScene(Scene& scene) override;
    void renderNode(SceneNode& node, const glm::mat4& parentTransform = glm::mat4(1.0f)) override {}
    void renderFromCamera(Scene& scene, CameraComponent* cam, const glm::vec4& vpNorm) override;

    void recordRenderCommands(vk::CommandBuffer cmdBuf, uint32_t imageIndex);
    void recordTextRenderCommand(vk::CommandBuffer cmdBuf, const TextRenderCommand& tc);

    // Set the current swapchain image index (frame owner `VulkanFrame` sets this)
    void setCurrentImageIndex(uint32_t idx) { currentImageIndex = idx; }

    void renderMesh(const Mesh& mesh, const Material& material, const glm::mat4& modelMatrix) override;
    void submitRenderCommand(const RenderCommand& command) override;
    void submitTextRenderCommand(const TextRenderCommand& command) override;

    std::vector<RenderCommand> renderQueue;
    std::vector<TextRenderCommand> textRenderQueue;

    void setActiveCamera(CameraComponent* camera) override { activeCamera = camera; }
    CameraComponent* getActiveCamera() const override { return activeCamera; }

    void setViewport(int x, int y, int width, int height) override {}
    glm::ivec4 getViewport() const override { return glm::ivec4(0); }

    void setClearColor(const glm::vec3& color) override {}
    void setClearColor(float r, float g, float b) override {}
    void clear() override {}

    void setWireframe(bool enabled) override {}
    void setDepthTest(bool enabled) override {}
    void setCullFace(bool enabled) override {}
    void setFrustumCulling(bool enabled) override {}
    bool isFrustumCullingEnabled() const override { return false; }

    void updateLightingUniforms() override;
    glm::vec3 extractCameraPosition(const glm::mat4& viewMatrix) override { return glm::vec3(glm::inverse(viewMatrix)[3]); }

    const RenderStats& getStats() const override { return stats; }
    void resetStats() override { stats.reset(); }

    void create(VulkanDevice* device, VulkanSwapChain* swapchain, VulkanResources* resources, VulkanPipeline* pipeline);

private:
    VulkanDevice* device_ = nullptr;
    VulkanSwapChain* swapChain_ = nullptr;
    VulkanResources* resources_ = nullptr;
    VulkanPipeline* pipeline_ = nullptr;

    uint32_t currentImageIndex = 0;
    CameraComponent* activeCamera = nullptr;
    RenderStats stats;
};

}
