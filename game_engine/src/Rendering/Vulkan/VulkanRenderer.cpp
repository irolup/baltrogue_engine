#include "Rendering/Vulkan/VulkanRenderer.h"
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Rendering/Material.h"
#include "Rendering/TextMaterial.h"
#include "Rendering/FontManager.h"
#include <limits>
#include <iostream>

namespace GameEngine {

void VulkanRenderer::create(VulkanDevice* device, VulkanSwapChain* swapchain, VulkanResources* resources, VulkanPipeline* pipeline) {
    device_ = device;
    swapChain_ = swapchain;
    resources_ = resources;
    pipeline_ = pipeline;
}

void VulkanRenderer::beginFrame() {
    if (!device_ || !swapChain_) return;
    renderQueue.clear();
    textRenderQueue.clear();
}

void VulkanRenderer::renderScene(Scene& scene) {
    auto camNode = scene.getActiveGameCamera();
    if (!camNode) return;
    auto cam = camNode->getComponent<CameraComponent>();
    if (!cam) return;
    renderFromCamera(scene, cam, cam->getViewport());
}

void VulkanRenderer::renderFromCamera(Scene& scene, CameraComponent* cam, const glm::vec4& vpNorm) {
    if (!cam || !resources_) return;
    activeCamera = cam;

    PerFrameUniforms ubo{};
    ubo.view = cam->getViewMatrix();
    ubo.proj = cam->getProjectionMatrix();
    glm::vec3 camPos = extractCameraPosition(ubo.view);
    ubo.cameraPosition = glm::vec4(camPos, 0.0f);

    // Map and copy to uniform buffer for current image
    if (resources_->getUniformBufferSize() > 0) {
        auto& mem = resources_->getUniformBufferMemory(currentImageIndex);
        void* data = mem.mapMemory(0, resources_->getUniformBufferSize());
        memcpy(data, &ubo, static_cast<size_t>(resources_->getUniformBufferSize()));
        mem.unmapMemory();
    }

    if (auto root = scene.getRootNode()) {
        root->render(*this);
    }
}

void VulkanRenderer::recordRenderCommands(vk::CommandBuffer cmdBuf, uint32_t imageIndex) {
    for (const auto& rc : renderQueue) {
        if (!rc.mesh) continue;
        const Mesh& mesh = *rc.mesh;
        const VulkanMeshGpu& vulkanMesh = resources_->getOrUploadMesh(mesh);

        if (vulkanMesh.vertexCount == 0) {
            continue;
        }

        vk::Buffer vb = static_cast<vk::Buffer>(*vulkanMesh.vertexBuffer);
        vk::DeviceSize offsets[] = {0};
        cmdBuf.bindVertexBuffers(0, 1, &vb, offsets);

        if (vulkanMesh.indexCount > 0) {
            cmdBuf.bindIndexBuffer(static_cast<vk::Buffer>(*vulkanMesh.indexBuffer), 0, vk::IndexType::eUint32);
        }

        // Bind material descriptor set (set 1)
        if (rc.material) {
            const auto& diffuseTex = resources_->getOrCreateTexture(rc.material->hasDiffuseTexture() ? rc.material->getDiffuseTexturePath() : std::string(""));
            vk::DescriptorSet matSet = pipeline_->getOrCreateMaterialDescriptorSet(rc.material.get(), diffuseTex);
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_->getGraphicsPipelineLayout(), 1, {matSet}, {});
        }

        auto layout = *pipeline_->getGraphicsPipelineLayout();
        PushConstants push{};
        push.modelMatrix = rc.modelMatrix;

        cmdBuf.pushConstants(
            layout,
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(PushConstants),
            &push
        );


        // Draw indexed when available, otherwise draw as a non-indexed mesh.
        if (vulkanMesh.indexCount > 0) {
            cmdBuf.drawIndexed(vulkanMesh.indexCount, 1, 0, 0, 0);
        } else {
            cmdBuf.draw(vulkanMesh.vertexCount, 1, 0, 0);
        }
    }
    if (!textRenderQueue.empty()) {
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_->getTextPipeline());
        cmdBuf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipeline_->getTextPipelineLayout(),
            0,
            {pipeline_->getDescriptorSet(imageIndex)},
            {});

        for (const auto& tc : textRenderQueue) {
            if (tc.renderMode == TextRenderMode::WORLD_SPACE) {
                recordTextRenderCommand(cmdBuf, tc);
            }
        }
        for (const auto& tc : textRenderQueue) {
            if (tc.renderMode == TextRenderMode::SCREEN_SPACE) {
                recordTextRenderCommand(cmdBuf, tc);
            }
        }
    }

}

void VulkanRenderer::recordTextRenderCommand(vk::CommandBuffer cmdBuf, const TextRenderCommand& tc){
    if (!tc.mesh && !tc.textComponent)
        return;

    (void)cmdBuf; (void)tc;

    uint32_t vertexCount = 0;
    uint32_t indexCount  = 0;

    if (tc.textComponent)
    {
        const VulkanTextMeshGpu& textGpu =
            resources_->getOrUploadTextMesh(*tc.textComponent);

        (void)textGpu;

        if (textGpu.vertexCount == 0)
            return;

        vk::Buffer vb = static_cast<vk::Buffer>(*textGpu.vertexBuffer);
        vk::DeviceSize offsets[] = { 0 };

        cmdBuf.bindVertexBuffers(0, 1, &vb, offsets);

        if (textGpu.indexCount > 0)
        {
            cmdBuf.bindIndexBuffer(
                static_cast<vk::Buffer>(*textGpu.indexBuffer),
                0,
                vk::IndexType::eUint32);
        }

        vertexCount = textGpu.vertexCount;
        indexCount  = textGpu.indexCount;

    }
    else
    {
        const VulkanMeshGpu& meshGpu =
            resources_->getOrUploadMesh(*tc.mesh);

        if (meshGpu.vertexCount == 0)
            return;

        vk::Buffer vb = static_cast<vk::Buffer>(*meshGpu.vertexBuffer);
        vk::DeviceSize offsets[] = { 0 };

        cmdBuf.bindVertexBuffers(0, 1, &vb, offsets);

        if (meshGpu.indexCount > 0)
        {
            cmdBuf.bindIndexBuffer(
                static_cast<vk::Buffer>(*meshGpu.indexBuffer),
                0,
                vk::IndexType::eUint32);
        }

        vertexCount = meshGpu.vertexCount;
        indexCount  = meshGpu.indexCount;
    }

    if (tc.material)
    {
        const auto& atlasTex =
            resources_->getOrCreateFontAtlasTexture(
                tc.material->getFontAtlas()->atlasData,
                tc.material->getFontAtlas()->atlasWidth,
                tc.material->getFontAtlas()->atlasHeight,
                tc.material->getFontAtlas()->cacheKey);

        vk::DescriptorSet matSet =
            pipeline_->getOrCreateTextDescriptorSet(
                tc.material.get(),
                atlasTex);

        cmdBuf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipeline_->getTextPipelineLayout(),
            1,
            { matSet },
            {});
    }

    PushConstants push{};
    push.modelMatrix = tc.modelMatrix;

    (void)vertexCount; (void)indexCount;

    cmdBuf.pushConstants(
        *pipeline_->getTextPipelineLayout(),
        vk::ShaderStageFlagBits::eVertex |
        vk::ShaderStageFlagBits::eFragment,
        0,
        sizeof(PushConstants),
        &push);

    if (indexCount > 0)
    {
        cmdBuf.drawIndexed(indexCount, 1, 0, 0, 0);
    }
    else
    {
        cmdBuf.draw(vertexCount, 1, 0, 0);
    }
}


void VulkanRenderer::endFrame() {
    // Presentation is handled by VulkanFrame renderer does not present here.
}

void VulkanRenderer::submitRenderCommand(const RenderCommand& command) {
    renderQueue.push_back(command);
}

void VulkanRenderer::submitTextRenderCommand(const TextRenderCommand& command) {
    textRenderQueue.push_back(command);
}

void VulkanRenderer::renderMesh(const Mesh& mesh, const Material& material, const glm::mat4& modelMatrix) {
    RenderCommand cmd;
    cmd.mesh = std::make_shared<Mesh>(mesh);
    cmd.material = std::make_shared<Material>(material);
    cmd.modelMatrix = modelMatrix;
    submitRenderCommand(cmd);
}

}
