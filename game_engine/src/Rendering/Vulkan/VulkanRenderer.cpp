#include "Rendering/Vulkan/VulkanRenderer.h"
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Components/SkyboxComponent.h"
#include "Rendering/LightingManager.h"
#include "Rendering/Material.h"
#include "Rendering/TextMaterial.h"
#include "Rendering/FontManager.h"
#include "Rendering/Shader.h"
#include "Core/Engine.h"
#include <algorithm>
#include <limits>

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
    animationSlots_.clear();
    cameraVisible_.clear();
    shadowDraws_.clear();
    stats.reset();
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

    auto& lightingManager = LightingManager::getInstance();
    lightingManager.update();
    lightingManager.beginPass();

    // Must run before the light array is read: it stamps each light with the
    // atlas tile its shadow lookup will use
    auto& shadowManager = ShadowManager::getInstance();
    shadowManager.update(camPos, cam->getForward());

    // Picks up a tile size changed from the editor. Same size is a cheap no-op.
    if (shadowManager.hasShadows()) {
        const uint32_t atlasWidth = static_cast<uint32_t>(shadowManager.getAtlasWidth());
        const uint32_t atlasHeight = static_cast<uint32_t>(shadowManager.getAtlasHeight());
        if (resources_->getShadowAtlasWidth() != atlasWidth || resources_->getShadowAtlasHeight() != atlasHeight) {
            if (resources_->ensureShadowAtlas(atlasWidth, atlasHeight)) {
                pipeline_->refreshShadowAtlasBinding();
            }
        }
    }

    const std::vector<glm::mat4>& shadowMatrices = shadowManager.getViewMatrices();
    ubo.numShadowViews = static_cast<int32_t>(shadowMatrices.size());
    ubo.shadowParams = shadowManager.getShaderParams();
    for (size_t i = 0; i < shadowMatrices.size() && i < kMaxShadowViews; ++i) {
        ubo.shadowMatrices[i] = shadowMatrices[i];
    }

    ubo.numLights = static_cast<int32_t>(lightingManager.getActiveLightCount());
    auto lightData = lightingManager.getLightDataArray();
    for (size_t i = 0; i < lightData.size() && i < lightingManager.MAX_LIGHTS; ++i) {
        ubo.lights[i].position    = lightData[i].position;
        ubo.lights[i].direction   = lightData[i].direction;
        ubo.lights[i].color       = lightData[i].color;
        ubo.lights[i].params      = lightData[i].params;
        ubo.lights[i].attenuation = lightData[i].attenuation;
    }

    resolveFrameEnvironment(scene);
    ubo.hasEnvironmentMap = frameEnvironment_.active ? 1 : 0;

    updateFrustum(ubo.view, ubo.proj);

    if (resources_->getUniformBufferSize() > 0) {
        resources_->writeUniformBuffer(
            currentImageIndex,
            &ubo,
            resources_->getUniformBufferSize());
    }

    if (auto root = scene.getRootNode()) {
        root->render(*this);
    }

    prepareRenderResources();
}

void VulkanRenderer::recordRenderCommands(vk::CommandBuffer cmdBuf, uint32_t imageIndex) {
    if (!pipeline_) {
        return;
    }

    recordSkyboxRenderCommand(cmdBuf, imageIndex);

    BlendMode boundBlendMode = BlendMode::Opaque;
    bool boundDepthWrite = true;
    bool boundCullEnabled = true;
    bool defaultLitBound = true;
    cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_->getGraphicsPipeline());
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipeline_->getGraphicsPipelineLayout(),
        static_cast<uint32_t>(SET_FRAME),
        {pipeline_->getDescriptorSet(imageIndex)},
        {});

    if (!environmentCubemap_) {
        return;
    }

    vk::DescriptorSet environmentSet = pipeline_->getOrUpdateEnvironmentDescriptorSet(
        frameEnvironment_, *environmentCubemap_);
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipeline_->getGraphicsPipelineLayout(),
        static_cast<uint32_t>(SET_ENVIRONMENT),
        {environmentSet},
        {});

    for (size_t drawIndex = 0; drawIndex < renderQueue.size(); ++drawIndex) {
        const auto& rc = renderQueue[drawIndex];
        if (!rc.mesh) continue;
        if (drawIndex < cameraVisible_.size() && cameraVisible_[drawIndex] == 0) continue;

        if (rc.isBeam || (rc.material && rc.material->getVulkanShaderPipelineKind() != VulkanShaderPipelineKind::DefaultLit)) {
            recordShaderMaterialRenderCommand(cmdBuf, imageIndex, rc, GetEngine().getTime().getTotalTime());
            defaultLitBound = false;
            continue;
        }

        const BlendMode blendMode = rc.material ? rc.material->getBlendMode() : BlendMode::Opaque;
        const bool depthWrite = rc.material ? rc.material->getDepthWrite() : true;
        const bool cullEnabled = !rc.disableCulling;
        if (!defaultLitBound || blendMode != boundBlendMode || depthWrite != boundDepthWrite
            || cullEnabled != boundCullEnabled) {
            cmdBuf.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                *pipeline_->getGraphicsPipeline(blendMode, depthWrite, cullEnabled));
            boundBlendMode = blendMode;
            boundDepthWrite = depthWrite;
            boundCullEnabled = cullEnabled;
            defaultLitBound = true;
            cmdBuf.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *pipeline_->getGraphicsPipelineLayout(),
                static_cast<uint32_t>(SET_FRAME),
                {pipeline_->getDescriptorSet(imageIndex)},
                {});
            cmdBuf.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *pipeline_->getGraphicsPipelineLayout(),
                static_cast<uint32_t>(SET_ENVIRONMENT),
                {environmentSet},
                {});
        }

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

        if (rc.material) {
            vk::DescriptorSet matSet = pipeline_->getOrCreateMaterialDescriptorSet(rc.material.get());
            cmdBuf.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *pipeline_->getGraphicsPipelineLayout(),
                static_cast<uint32_t>(SET_MATERIAL),
                {matSet},
                {});
        }

        const uint32_t animationSlot = (drawIndex < animationSlots_.size())
            ? animationSlots_[drawIndex]
            : 0u;
        cmdBuf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipeline_->getGraphicsPipelineLayout(),
            static_cast<uint32_t>(SET_ANIMATION),
            {pipeline_->getAnimationDescriptorSet(animationSlot)},
            {});

        auto layout = *pipeline_->getGraphicsPipelineLayout();
        PushConstants push{};
        push.modelMatrix = rc.modelMatrix;
        push.receiveShadows = (rc.receiveShadows && !shadowDraws_.empty()) ? 1 : 0;

        cmdBuf.pushConstants(
            layout,
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(PushConstants),
            &push);

        if (vulkanMesh.indexCount > 0) {
            cmdBuf.drawIndexed(vulkanMesh.indexCount, 1, 0, 0, 0);
        } else {
            cmdBuf.draw(vulkanMesh.vertexCount, 1, 0, 0);
        }

        stats.drawCalls++;
        stats.triangles += static_cast<int>(mesh.getTriangleCount());
        stats.vertices += static_cast<int>(mesh.getVertexCount());
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

    uint32_t vertexCount = 0;
    uint32_t indexCount  = 0;

    if (tc.textComponent)
    {
        const VulkanTextMeshGpu* textGpu = resources_->findTextMesh(*tc.textComponent);
        if (!textGpu || textGpu->vertexCount == 0) {
            return;
        }

        vk::Buffer vb = static_cast<vk::Buffer>(*textGpu->vertexBuffer);
        vk::DeviceSize offsets[] = { 0 };

        cmdBuf.bindVertexBuffers(0, 1, &vb, offsets);

        if (textGpu->indexCount > 0)
        {
            cmdBuf.bindIndexBuffer(
                static_cast<vk::Buffer>(*textGpu->indexBuffer),
                0,
                vk::IndexType::eUint32);
        }

        vertexCount = textGpu->vertexCount;
        indexCount  = textGpu->indexCount;
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

void VulkanRenderer::recordSkyboxRenderCommand(vk::CommandBuffer cmdBuf, uint32_t imageIndex) {
    if (!pipeline_ || !frameEnvironment_.active || !environmentCubemap_) {
        return;
    }

    const VulkanMeshGpu& skyboxMesh = resources_->getSkyboxMesh();
    if (skyboxMesh.vertexCount == 0) {
        return;
    }

    cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_->getSkyboxPipeline());

    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipeline_->getSkyboxPipelineLayout(),
        static_cast<uint32_t>(SET_FRAME),
        {pipeline_->getDescriptorSet(imageIndex)},
        {});

    vk::DescriptorSet environmentSet = pipeline_->getOrUpdateEnvironmentDescriptorSet(
        frameEnvironment_, *environmentCubemap_);
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipeline_->getSkyboxPipelineLayout(),
        1,
        {environmentSet},
        {});

    vk::Buffer vb = static_cast<vk::Buffer>(*skyboxMesh.vertexBuffer);
    vk::DeviceSize offsets[] = {0};
    cmdBuf.bindVertexBuffers(0, 1, &vb, offsets);
    cmdBuf.draw(skyboxMesh.vertexCount, 1, 0, 0);

    stats.drawCalls++;
    stats.triangles += 12;
}

void VulkanRenderer::recordShaderMaterialRenderCommand(
    vk::CommandBuffer cmdBuf,
    uint32_t imageIndex,
    const RenderCommand& rc,
    float totalTime)
{
    if (!pipeline_ || !rc.mesh || !rc.material) {
        return;
    }

    const Material& material = *rc.material;
    const VulkanShaderPipelineKind pipelineKind = material.getVulkanShaderPipelineKind();
    const bool isBeamDraw = rc.isBeam || pipelineKind == VulkanShaderPipelineKind::Beam;

    vk::Pipeline pipeline = VK_NULL_HANDLE;
    vk::PipelineLayout layout = VK_NULL_HANDLE;

    if (isBeamDraw) {
        pipeline = static_cast<vk::Pipeline>(*pipeline_->getBeamPipeline());
        layout = static_cast<vk::PipelineLayout>(*pipeline_->getBeamPipelineLayout());
    } else if (pipelineKind == VulkanShaderPipelineKind::Custom) {
        const VulkanPipeline::CachedShaderPipeline* customPipeline = pipeline_->getCustomShaderPipeline(&material);
        if (!customPipeline) {
            return;
        }
        pipeline = static_cast<vk::Pipeline>(*customPipeline->pipeline);
        layout = static_cast<vk::PipelineLayout>(*customPipeline->layout);
    } else {
        return;
    }

    const VulkanMeshGpu& vulkanMesh = resources_->getOrUploadMesh(*rc.mesh);
    if (vulkanMesh.vertexCount == 0) {
        return;
    }

    cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        layout,
        static_cast<uint32_t>(SET_FRAME),
        {pipeline_->getDescriptorSet(imageIndex)},
        {});

    vk::DescriptorSet materialSet = pipeline_->getOrCreateShaderMaterialDescriptorSet(rc.material.get());
    cmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        layout,
        1,
        {materialSet},
        {});

    vk::Buffer vb = static_cast<vk::Buffer>(*vulkanMesh.vertexBuffer);
    vk::DeviceSize offsets[] = {0};
    cmdBuf.bindVertexBuffers(0, 1, &vb, offsets);

    if (vulkanMesh.indexCount > 0) {
        cmdBuf.bindIndexBuffer(static_cast<vk::Buffer>(*vulkanMesh.indexBuffer), 0, vk::IndexType::eUint32);
    }

    if (isBeamDraw) {
        BeamPushConstants beamPush{};
        beamPush.beamStart = glm::vec4(rc.beamStart, 0.0f);
        beamPush.beamEnd = glm::vec4(rc.beamEnd, 0.0f);
        beamPush.beamHalfWidth = rc.beamHalfWidth;
        beamPush.time = totalTime;
        cmdBuf.pushConstants(
            layout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(BeamPushConstants),
            &beamPush);
    } else {
        PushConstants push{};
        push.modelMatrix = rc.modelMatrix;
        cmdBuf.pushConstants(
            layout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(PushConstants),
            &push);
    }

    if (vulkanMesh.indexCount > 0) {
        cmdBuf.drawIndexed(vulkanMesh.indexCount, 1, 0, 0, 0);
    } else {
        cmdBuf.draw(vulkanMesh.vertexCount, 1, 0, 0);
    }

    stats.drawCalls++;
    stats.triangles += static_cast<int>(rc.mesh->getTriangleCount());
    stats.vertices += static_cast<int>(rc.mesh->getVertexCount());
}

void VulkanRenderer::endFrame() {
    // Presentation is handled by VulkanFrame; renderer does not present here.
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

void VulkanRenderer::updateLightingUniforms(){
    LightingManager::getInstance().update();
}

void VulkanRenderer::resolveFrameEnvironment(Scene& scene) {
    frameEnvironment_.active = false;
    environmentCubemap_ = &resources_->getDefaultCubemapTexture();

    if (auto node = scene.getActiveSkybox()) {
        if (auto* skybox = node->getComponent<SkyboxComponent>(); skybox && skybox->isActive()) {
            environmentCubemap_ = &resources_->getOrCreateCubemapTexture(skybox->getTexturePaths());
            frameEnvironment_.active = true;
        }
    }

    frameEnvironment_.cacheKey = environmentCubemap_;
}

void VulkanRenderer::prepareRenderResources() {
    sortRenderQueue();
    cullRenderQueue();
    buildShadowDrawList();

    if (frameEnvironment_.active && environmentCubemap_) {
        resources_->getSkyboxMesh();
        pipeline_->getOrUpdateEnvironmentDescriptorSet(frameEnvironment_, *environmentCubemap_);
    }

    for (const auto& rc : renderQueue) {
        if (rc.mesh) {
            resources_->getOrUploadMesh(*rc.mesh);
        }
        if (rc.material) {
            if (rc.isBeam || rc.material->getVulkanShaderPipelineKind() != VulkanShaderPipelineKind::DefaultLit) {
                pipeline_->getOrCreateShaderMaterialDescriptorSet(rc.material.get());
                if (rc.material->getVulkanShaderPipelineKind() == VulkanShaderPipelineKind::Custom) {
                    pipeline_->getCustomShaderPipeline(rc.material.get());
                }
            } else {
                pipeline_->getOrCreateMaterialDescriptorSet(rc.material.get());
            }
        }
    }

    for (const auto& tc : textRenderQueue) {
        if (tc.textComponent) {
            resources_->getOrUploadTextMesh(*tc.textComponent);
        } else if (tc.mesh) {
            resources_->getOrUploadMesh(*tc.mesh);
        }
        if (tc.material && tc.material->getFontAtlas()) {
            const auto& atlas = tc.material->getFontAtlas();
            const auto& atlasTex = resources_->getOrCreateFontAtlasTexture(
                atlas->atlasData,
                atlas->atlasWidth,
                atlas->atlasHeight,
                atlas->cacheKey);
            pipeline_->getOrCreateTextDescriptorSet(tc.material.get(), atlasTex);
        }
    }

    prepareAnimationResources();
    resources_->waitForUploads();
}

void VulkanRenderer::prepareAnimationResources() {
    animationSlots_.clear();
    animationSlots_.reserve(renderQueue.size());

    // Slots are grouped per swapchain image (see createAnimationUniformBuffers)
    const uint32_t slotBase = currentImageIndex * kMaxSkinnedDrawsPerFrame;

    resources_->writeAnimationUniform(slotBase, {});

    std::vector<uint8_t> neededForShadows(renderQueue.size(), 0);
    for (const ShadowDrawItem& item : shadowDraws_) {
        if (item.queueIndex < neededForShadows.size()) {
            neededForShadows[item.queueIndex] = 1;
        }
    }

    uint32_t nextSlot = 1;
    for (size_t i = 0; i < renderQueue.size(); ++i) {
        const RenderCommand& rc = renderQueue[i];
        const bool visible = (i < cameraVisible_.size()) && cameraVisible_[i] != 0;

        if (!rc.boneTransforms || rc.boneTransforms->empty()
            || nextSlot >= kMaxSkinnedDrawsPerFrame
            || (!visible && !neededForShadows[i])) {
            animationSlots_.push_back(slotBase);
            continue;
        }

        resources_->writeAnimationUniform(slotBase + nextSlot, *rc.boneTransforms);
        animationSlots_.push_back(slotBase + nextSlot);
        ++nextSlot;
    }
}

void VulkanRenderer::sortRenderQueue() {
    std::sort(renderQueue.begin(), renderQueue.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            if (!a.material && !b.material) return false;
            if (!a.material) return false;
            if (!b.material) return true;
            const bool aOpaque = (a.material->getBlendMode() == BlendMode::Opaque);
            const bool bOpaque = (b.material->getBlendMode() == BlendMode::Opaque);
            if (aOpaque != bOpaque) {
                return aOpaque;
            }
            const auto shaderA = a.material->getShader();
            const auto shaderB = b.material->getShader();
            if (shaderA != shaderB) {
                return shaderA.get() < shaderB.get();
            }
            return a.material.get() < b.material.get();
        });
}

void VulkanRenderer::cullRenderQueue() {
    // Marked rather than compacted: a caster outside the camera frustum can
    // still cast into it, so the shadow pass needs to see the whole queue
    cameraVisible_.assign(renderQueue.size(), 1);

    if (!frustumCullingEnabled_ || !activeCamera) {
        return;
    }

    for (size_t i = 0; i < renderQueue.size(); ++i) {
        const RenderCommand& command = renderQueue[i];
        if (!command.mesh || command.isBeam) {
            continue;
        }

        const glm::vec3 boundsMin = command.mesh->getBoundsMin();
        const glm::vec3 boundsMax = command.mesh->getBoundsMax();
        if (!Frustum::areBoundsValid(boundsMin, boundsMax)) {
            continue;
        }

        stats.totalObjectsTested++;
        if (!cameraFrustum_.containsAABB(boundsMin, boundsMax, command.modelMatrix)) {
            stats.culledObjects++;
            cameraVisible_[i] = 0;
        }
    }
}

void VulkanRenderer::buildShadowDrawList() {
    shadowDraws_.clear();

    const std::vector<ShadowView>& views = ShadowManager::getInstance().getViews();
    if (views.empty()) {
        return;
    }

    for (size_t i = 0; i < renderQueue.size(); ++i) {
        const RenderCommand& command = renderQueue[i];
        if (!command.mesh || !command.castShadows || command.isBeam) {
            continue;
        }

        const glm::vec3 boundsMin = command.mesh->getBoundsMin();
        const glm::vec3 boundsMax = command.mesh->getBoundsMax();
        const bool boundsValid = Frustum::areBoundsValid(boundsMin, boundsMax);

        uint32_t viewMask = 0;
        for (size_t v = 0; v < views.size(); ++v) {
            if (!boundsValid || views[v].frustum.containsAABB(boundsMin, boundsMax, command.modelMatrix)) {
                viewMask |= (1u << v);
            }
        }

        if (viewMask != 0) {
            ShadowDrawItem item;
            item.queueIndex = static_cast<uint32_t>(i);
            item.views = viewMask;
            shadowDraws_.push_back(item);
        }
    }
}

void VulkanRenderer::recordShadowPass(vk::CommandBuffer cmdBuf, uint32_t imageIndex) {
    if (!pipeline_ || !resources_ || shadowDraws_.empty()) {
        return;
    }

    auto& shadowManager = ShadowManager::getInstance();
    const std::vector<ShadowView>& views = shadowManager.getViews();
    if (views.empty() || !resources_->hasShadowAtlas() || !*pipeline_->getShadowPipeline()) {
        return;
    }

    vk::ImageMemoryBarrier toAttachment{};
    toAttachment.oldLayout = vk::ImageLayout::eUndefined; // cleared below, previous contents are dead
    toAttachment.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    toAttachment.srcAccessMask = {};
    toAttachment.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    toAttachment.image = resources_->getShadowAtlasImage();
    toAttachment.subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};
    cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                           vk::PipelineStageFlagBits::eEarlyFragmentTests,
                           {}, {}, {}, toAttachment);

    vk::RenderingAttachmentInfo depthAttachment{};
    depthAttachment.imageView = *resources_->getShadowAtlasImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue = vk::ClearValue(vk::ClearDepthStencilValue{1.0f, 0});

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderingInfo.renderArea.extent = vk::Extent2D{resources_->getShadowAtlasWidth(), resources_->getShadowAtlasHeight()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pDepthAttachment = &depthAttachment;

    cmdBuf.beginRendering(renderingInfo);
    cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_->getShadowPipeline());

    auto layout = *pipeline_->getGraphicsPipelineLayout();
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout,
                              static_cast<uint32_t>(SET_FRAME),
                              {pipeline_->getDescriptorSet(imageIndex)}, {});

    for (size_t v = 0; v < views.size(); ++v) {
        const glm::ivec4 tile = shadowManager.getTileViewport(views[v].tile);

        vk::Viewport viewport{};
        viewport.x = static_cast<float>(tile.x);
        viewport.y = static_cast<float>(tile.y);
        viewport.width = static_cast<float>(tile.z);
        viewport.height = static_cast<float>(tile.w);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmdBuf.setViewport(0, {viewport});

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{tile.x, tile.y};
        scissor.extent = vk::Extent2D{static_cast<uint32_t>(tile.z), static_cast<uint32_t>(tile.w)};
        cmdBuf.setScissor(0, {scissor});

        for (const ShadowDrawItem& item : shadowDraws_) {
            if ((item.views & (1u << v)) == 0) {
                continue;
            }

            const RenderCommand& rc = renderQueue[item.queueIndex];
            const VulkanMeshGpu& vulkanMesh = resources_->getOrUploadMesh(*rc.mesh);
            if (vulkanMesh.vertexCount == 0) {
                continue;
            }

            vk::Buffer vb = static_cast<vk::Buffer>(*vulkanMesh.vertexBuffer);
            vk::DeviceSize offsets[] = {0};
            cmdBuf.bindVertexBuffers(0, 1, &vb, offsets);
            if (vulkanMesh.indexCount > 0) {
                cmdBuf.bindIndexBuffer(static_cast<vk::Buffer>(*vulkanMesh.indexBuffer), 0, vk::IndexType::eUint32);
            }

            const uint32_t animationSlot = (item.queueIndex < animationSlots_.size())
                ? animationSlots_[item.queueIndex]
                : 0u;
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout,
                                      static_cast<uint32_t>(SET_ANIMATION),
                                      {pipeline_->getAnimationDescriptorSet(animationSlot)}, {});

            PushConstants push{};
            push.modelMatrix = rc.modelMatrix;
            push.shadowViewIndex = static_cast<int>(views[v].tile);
            cmdBuf.pushConstants(layout,
                                 vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                                 0, sizeof(PushConstants), &push);

            if (vulkanMesh.indexCount > 0) {
                cmdBuf.drawIndexed(vulkanMesh.indexCount, 1, 0, 0, 0);
            } else {
                cmdBuf.draw(vulkanMesh.vertexCount, 1, 0, 0);
            }

            stats.drawCalls++;
            stats.triangles += static_cast<int>(rc.mesh->getTriangleCount());
        }
    }

    cmdBuf.endRendering();

    vk::ImageMemoryBarrier toSampled{};
    toSampled.oldLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    toSampled.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    toSampled.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    toSampled.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    toSampled.image = resources_->getShadowAtlasImage();
    toSampled.subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};
    cmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests,
                           vk::PipelineStageFlagBits::eFragmentShader,
                           {}, {}, {}, toSampled);
}

void VulkanRenderer::updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!activeCamera) {
        return;
    }

    cameraFrustum_.update(projectionMatrix * viewMatrix);
}

}
