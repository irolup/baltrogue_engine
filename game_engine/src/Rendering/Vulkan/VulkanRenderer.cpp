#include "Rendering/Vulkan/VulkanRenderer.h"
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Components/SkyboxComponent.h"
#include "Rendering/LightingManager.h"
#include "Rendering/Material.h"
#include "Rendering/TextMaterial.h"
#include "Rendering/FontManager.h"
#include "Rendering/Shader.h"
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
    if (!environmentCubemap_ || !pipeline_) {
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

        if (rc.material) {
            vk::DescriptorSet matSet = pipeline_->getOrCreateMaterialDescriptorSet(rc.material.get());
            cmdBuf.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *pipeline_->getGraphicsPipelineLayout(),
                static_cast<uint32_t>(SET_MATERIAL),
                {matSet},
                {});
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
        const VulkanTextMeshGpu& textGpu =
            resources_->getOrUploadTextMesh(*tc.textComponent);

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
    static constexpr const char* kDefaultCubemapCacheKey = "__default_cubemap__";

    frameEnvironment_.active = false;
    frameEnvironment_.cacheKey = kDefaultCubemapCacheKey;
    environmentCubemap_ = &resources_->getDefaultCubemapTexture();

    if (auto node = scene.getActiveSkybox()) {
        if (auto* skybox = node->getComponent<SkyboxComponent>(); skybox && skybox->isActive()) {
            const auto& paths = skybox->getTexturePaths();
            environmentCubemap_ = &resources_->getOrCreateCubemapTexture(paths);
            frameEnvironment_.cacheKey = resources_->getCubemapCacheKey(paths);
            frameEnvironment_.active = true;
        }
    }
}

void VulkanRenderer::prepareRenderResources() {
    sortRenderQueue();
    cullRenderQueue();

    for (const auto& rc : renderQueue) {
        if (rc.mesh) {
            resources_->getOrUploadMesh(*rc.mesh);
        }
        if (rc.material) {
            pipeline_->getOrCreateMaterialDescriptorSet(rc.material.get());
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

    resources_->waitForUploads();
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
    if (!frustumCullingEnabled_ || !activeCamera) {
        return;
    }

    renderQueue.erase(
        std::remove_if(renderQueue.begin(), renderQueue.end(),
            [this](const RenderCommand& command) {
                if (!command.mesh || command.isBeam) {
                    return false;
                }

                const glm::vec3 boundsMin = command.mesh->getBoundsMin();
                const glm::vec3 boundsMax = command.mesh->getBoundsMax();
                bool boundsValid = (boundsMin.x < boundsMax.x && boundsMin.y < boundsMax.y && boundsMin.z < boundsMax.z);

                if (boundsValid) {
                    const float maxVal = std::numeric_limits<float>::max();
                    const float minVal = std::numeric_limits<float>::lowest();
                    if (boundsMin.x > maxVal * 0.1f || boundsMax.x < minVal * 0.1f) {
                        boundsValid = false;
                    }
                }

                if (!boundsValid) {
                    return false;
                }

                stats.totalObjectsTested++;
                if (!isAABBInFrustum(boundsMin, boundsMax, command.modelMatrix)) {
                    stats.culledObjects++;
                    return true;
                }
                return false;
            }),
        renderQueue.end());
}

void VulkanRenderer::updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!activeCamera) {
        return;
    }

    const glm::mat4 viewProj = projectionMatrix * viewMatrix;

    frustumPlanes_[0].normal = glm::vec3(viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0], viewProj[2][3] + viewProj[2][0]);
    frustumPlanes_[0].distance = viewProj[3][3] + viewProj[3][0];

    frustumPlanes_[1].normal = glm::vec3(viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0], viewProj[2][3] - viewProj[2][0]);
    frustumPlanes_[1].distance = viewProj[3][3] - viewProj[3][0];

    frustumPlanes_[2].normal = glm::vec3(viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1], viewProj[2][3] + viewProj[2][1]);
    frustumPlanes_[2].distance = viewProj[3][3] + viewProj[3][1];

    frustumPlanes_[3].normal = glm::vec3(viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1], viewProj[2][3] - viewProj[2][1]);
    frustumPlanes_[3].distance = viewProj[3][3] - viewProj[3][1];

    frustumPlanes_[4].normal = glm::vec3(viewProj[0][3] + viewProj[0][2], viewProj[1][3] + viewProj[1][2], viewProj[2][3] + viewProj[2][2]);
    frustumPlanes_[4].distance = viewProj[3][3] + viewProj[3][2];

    frustumPlanes_[5].normal = glm::vec3(viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2], viewProj[2][3] - viewProj[2][2]);
    frustumPlanes_[5].distance = viewProj[3][3] - viewProj[3][2];

    constexpr float epsilon = 0.0001f;
    for (auto& plane : frustumPlanes_) {
        const float length = glm::length(plane.normal);
        if (length > epsilon) {
            plane.normal /= length;
            plane.distance /= length;
        }
    }
}

bool VulkanRenderer::isAABBInFrustum(const glm::vec3& min, const glm::vec3& max, const glm::mat4& transform) const {
    if (min.x >= max.x || min.y >= max.y || min.z >= max.z) {
        return true;
    }

    glm::vec3 corners[8] = {
        glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f)),
        glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f)),
    };

    for (const auto& plane : frustumPlanes_) {
        bool inside = false;
        constexpr float margin = -0.1f;
        for (const auto& corner : corners) {
            if (glm::dot(plane.normal, corner) + plane.distance > margin) {
                inside = true;
                break;
            }
        }
        if (!inside) {
            return false;
        }
    }
    return true;
}

}
