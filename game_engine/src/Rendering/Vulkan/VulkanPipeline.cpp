#include "Rendering/Vulkan/VulkanPipeline.h"
#include "Core/AssetPaths.h"
#include "Rendering/InstanceBatcher.h"
#include "Rendering/Material.h"
#include "Rendering/TextMaterial.h"
#include "Rendering/ShadowMap.h"
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <glm/glm.hpp>

namespace GameEngine {

namespace {
constexpr const char* kDefaultVertexShaderSpvPath = "assets/vulkan/default_lit.vert.spv";
constexpr const char* kDefaultInstancedVertexShaderSpvPath = "assets/vulkan/default_lit_instanced.vert.spv";
constexpr const char* kDefaultFragmentShaderSpvPath = "assets/vulkan/default_lit.frag.spv";
constexpr const char* kTextVertexShaderSpvPath = "assets/vulkan/text.vert.spv";
constexpr const char* kTextFragmentShaderSpvPath = "assets/vulkan/text.frag.spv";
constexpr const char* kSkyboxVertexShaderSpvPath = "assets/vulkan/skybox.vert.spv";
constexpr const char* kSkyboxFragmentShaderSpvPath = "assets/vulkan/skybox.frag.spv";
constexpr const char* kBeamVertexShaderSpvPath = "assets/vulkan/beam.vert.spv";
constexpr const char* kBeamFragmentShaderSpvPath = "assets/vulkan/beam.frag.spv";
constexpr const char* kShadowVertexShaderSpvPath = "assets/vulkan/shadow_depth.vert.spv";
constexpr const char* kShadowFragmentShaderSpvPath = "assets/vulkan/shadow_depth.frag.spv";
}

void VulkanPipeline::create(VulkanDevice& device, VulkanResources& vulkanResources, VulkanSwapChain& swapchain){
    device_ = &device;
    resources_ = &vulkanResources;
    swapChain_ = &swapchain;

    createDescriptorSetLayout();
    createShaderMaterialDescriptorSetLayout();
    createCustomTextureDescriptorSetLayout();
    createAnimationDescriptorSetLayout();
    createTextDescriptorSetLayout();

    createGraphicsPipeline();
    createInstancedGraphicsPipeline();
    createTextPipeline(false, textPipeline_);
    createTextPipeline(true, worldTextPipeline_);
    createSkyboxPipeline();
    createBeamPipeline();

    // The shadow pipeline needs the atlas format, and the lit shader references
    // the atlas unconditionally so the descriptor must be valid from the start
    const auto& shadowSettings = ShadowManager::getInstance().getSettings();
    resources_->ensureShadowAtlas(
        static_cast<uint32_t>(shadowSettings.tileSize * static_cast<int>(kShadowAtlasCols)),
        static_cast<uint32_t>(shadowSettings.tileSize * static_cast<int>(kShadowAtlasRows)));
    createShadowPipeline();

    createDescriptorPoolAndSets();
    if (ENABLE_PARTICLE_COMPUTE){
        createParticleGraphicsPipeline();
    }
}

void VulkanPipeline::createDescriptorSetLayout() {
    // Frame descriptor layout: binding 0 = UBO, binding 1 = shadow atlas
    std::array frameBindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo frameLayoutInfo{};
    frameLayoutInfo.bindingCount = static_cast<uint32_t>(frameBindings.size());
    frameLayoutInfo.pBindings = frameBindings.data();
    frameDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(device_->getDevice(), frameLayoutInfo);

    // Material descriptor layout: binding 0 = diffuse, 1 = normal, 2 = ARM, 3 = material UBO
    std::array materialBindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo materialLayoutInfo{};
    materialLayoutInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
    materialLayoutInfo.pBindings = materialBindings.data();
    materialDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(device_->getDevice(), materialLayoutInfo);

    // Environment descriptor layout: binding 0 = skybox cubemap (shared per frame)
    std::array environmentBindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo environmentLayoutInfo{};
    environmentLayoutInfo.bindingCount = static_cast<uint32_t>(environmentBindings.size());
    environmentLayoutInfo.pBindings = environmentBindings.data();
    environmentDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(device_->getDevice(), environmentLayoutInfo);
}

void VulkanPipeline::createShaderMaterialDescriptorSetLayout() {
    // Beam-only material set: diffuse + up to 3 custom textures + small UBO.
    std::array shaderMaterialBindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(shaderMaterialBindings.size());
    layoutInfo.pBindings = shaderMaterialBindings.data();
    shaderMaterialDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(device_->getDevice(), layoutInfo);
}

void VulkanPipeline::createCustomTextureDescriptorSetLayout() {
    std::array customTextureBindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(customTextureBindings.size());
    layoutInfo.pBindings = customTextureBindings.data();
    customTextureDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(device_->getDevice(), layoutInfo);
}

void VulkanPipeline::createAnimationDescriptorSetLayout() {
    std::array animationBindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo animationLayoutInfo{};
    animationLayoutInfo.bindingCount = static_cast<uint32_t>(animationBindings.size());
    animationLayoutInfo.pBindings = animationBindings.data();
    animationDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(device_->getDevice(), animationLayoutInfo);
}

void VulkanPipeline::createTextDescriptorSetLayout() {
    // Material descriptor layout: binding 0 = textureAtlas, 1  = TextMaterialUniforms UBO
    std::array materialBindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
    layoutInfo.pBindings = materialBindings.data();

    textDescriptorSetLayout_ = vk::raii::DescriptorSetLayout( device_->getDevice(),layoutInfo);
}

void VulkanPipeline::createDescriptorPoolAndSets() {
    // create pool for uniform buffers (one per swapchain image)
    uint32_t imageCount = static_cast<uint32_t>(swapChain_->getImages().size());
    const uint32_t maxMaterialsPerScene = 128;
    uint32_t materialDescriptorCapacity = maxMaterialsPerScene * imageCount;
    uint32_t shaderMaterialDescriptorCapacity = std::max<uint32_t>(imageCount * 64, imageCount);
    uint32_t customTextureDescriptorCapacity = shaderMaterialDescriptorCapacity;
    uint32_t animationDescriptorCapacity = kMaxSkinnedDrawsPerFrame * imageCount;
    const uint32_t maxTextMaterialsPerScene = 16;
    uint32_t textMaterialDescriptorCapacity = maxTextMaterialsPerScene * imageCount;
    std::array poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, imageCount + materialDescriptorCapacity + shaderMaterialDescriptorCapacity + animationDescriptorCapacity + textMaterialDescriptorCapacity),
        // + imageCount for the shadow atlas bound alongside each frame UBO.
        vk::DescriptorPoolSize(
            vk::DescriptorType::eCombinedImageSampler,
            materialDescriptorCapacity * 3
                + shaderMaterialDescriptorCapacity * 4
                + customTextureDescriptorCapacity * 3
                + textMaterialDescriptorCapacity
                + 1
                + imageCount)
    };

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = imageCount
        + materialDescriptorCapacity
        + shaderMaterialDescriptorCapacity
        + customTextureDescriptorCapacity
        + textMaterialDescriptorCapacity
        + 1
        + animationDescriptorCapacity;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    materialDescriptorSets_.clear();
    shaderMaterialDescriptorSets_.clear();
    customTextureDescriptorSets_.clear();
    textMaterialDescriptorSets_.clear();
    descriptorSets_.clear();
    animationDescriptorSets_.clear();
    environmentDescriptorSet_.reset();
    currentEnvironmentKey_ = nullptr;
    descriptorPool_ = vk::raii::DescriptorPool(device_->getDevice(), poolInfo);

    // allocate descriptor sets
    std::vector<vk::DescriptorSetLayout> layouts(imageCount, *frameDescriptorSetLayout_);
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *descriptorPool_;
    allocInfo.descriptorSetCount = imageCount;
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets_.clear();
    descriptorSets_ = device_->getDevice().allocateDescriptorSets(allocInfo);

    // write per-frame UBO info
    vk::DeviceSize ubSize = resources_->getUniformBufferSize();
    for (uint32_t i = 0; i < imageCount; ++i) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = *resources_->getUniformBuffer(i);
        bufferInfo.offset = 0;
        bufferInfo.range = ubSize;

        vk::WriteDescriptorSet write{};
        write.dstSet = *descriptorSets_[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = vk::DescriptorType::eUniformBuffer;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        device_->getDevice().updateDescriptorSets({write}, {});
    }

    refreshShadowAtlasBinding();

    createAnimationDescriptorSets();
}

void VulkanPipeline::refreshShadowAtlasBinding() {
    if (!resources_->hasShadowAtlas()) {
        return;
    }

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.sampler = resources_->getShadowAtlasSampler();
    imageInfo.imageView = *resources_->getShadowAtlasImageView();
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    for (auto& descriptorSet : descriptorSets_) {
        vk::WriteDescriptorSet write{};
        write.dstSet = *descriptorSet;
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        device_->getDevice().updateDescriptorSets({write}, {});
    }
}

void VulkanPipeline::createAnimationDescriptorSets() {
    const uint32_t imageCount = static_cast<uint32_t>(swapChain_->getImages().size());
    const uint32_t totalSlots = kMaxSkinnedDrawsPerFrame * imageCount;

    animationDescriptorSets_.clear();
    animationDescriptorSets_.reserve(totalSlots);

    for (uint32_t slot = 0; slot < totalSlots; ++slot) {
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = *descriptorPool_;
        vk::DescriptorSetLayout layout = *animationDescriptorSetLayout_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        auto sets = device_->getDevice().allocateDescriptorSets(allocInfo);
        vk::raii::DescriptorSet set = std::move(sets.front());

        vk::DescriptorBufferInfo bufferInfo = resources_->getAnimationDescriptorBufferInfo(slot);

        vk::WriteDescriptorSet write{};
        write.dstSet = *set;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = vk::DescriptorType::eUniformBuffer;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        device_->getDevice().updateDescriptorSets({write}, {});
        animationDescriptorSets_.push_back(std::move(set));
    }
}

vk::DescriptorSet VulkanPipeline::getAnimationDescriptorSet(uint32_t slot) const {
    if (slot >= animationDescriptorSets_.size()) {
        return nullptr;
    }
    return *animationDescriptorSets_[slot];
}

MaterialUniforms VulkanPipeline::makeMaterialUniforms(const Material& material) {
    MaterialUniforms uniforms{};
    uniforms.baseColor = glm::vec4(material.getColorLinear(), material.getOpacity());
    uniforms.roughness = material.getRoughness();
    uniforms.metallic = material.getMetallic();
    uniforms.reflectionStrength = material.getReflectionStrength();
    uniforms.alphaCutoff = material.getAlphaCutoff();
    uniforms.textureFlags = glm::vec4(
        material.hasDiffuseTexture() ? 1.0f : 0.0f,
        material.hasNormalTexture() ? 1.0f : 0.0f,
        material.hasARMTexture() ? 1.0f : 0.0f,
        0.0f);
    uniforms.uvScaleOffset = glm::vec4(material.getUVScale(), material.getUVOffset());
    return uniforms;
}

void VulkanPipeline::writeMaterialDescriptorSet(vk::DescriptorSet set, const Material* material, uint32_t imageIndex) {
    if (material) {
        resources_->ensureMaterialUniformBuffer(material, imageIndex, makeMaterialUniforms(*material));
    }

    const auto& defaultTexture = resources_->getOrCreateTexture("");

    const auto& diffuseTexture = (material && material->hasDiffuseTexture())
        ? resources_->getOrCreateTexture(material->getDiffuseTexturePath())
        : defaultTexture;
    const auto& normalTexture = (material && material->hasNormalTexture())
        ? resources_->getOrCreateTexture(material->getNormalTexturePath())
        : defaultTexture;
    const auto& armTexture = (material && material->hasARMTexture())
        ? resources_->getOrCreateTexture(material->getARMTexturePath())
        : defaultTexture;

    vk::DescriptorImageInfo diffuseInfo{};
    diffuseInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    diffuseInfo.imageView = *diffuseTexture.view;
    diffuseInfo.sampler = *diffuseTexture.sampler;

    vk::DescriptorImageInfo normalInfo{};
    normalInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    normalInfo.imageView = *normalTexture.view;
    normalInfo.sampler = *normalTexture.sampler;

    vk::DescriptorImageInfo armInfo{};
    armInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    armInfo.imageView = *armTexture.view;
    armInfo.sampler = *armTexture.sampler;

    vk::DescriptorBufferInfo bufferInfo = resources_->getMaterialDescriptorBufferInfo(material, imageIndex);

    std::array<vk::WriteDescriptorSet, 4> writes{};

    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &diffuseInfo;

    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &normalInfo;

    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &armInfo;

    writes[3].dstSet = set;
    writes[3].dstBinding = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = vk::DescriptorType::eUniformBuffer;
    writes[3].descriptorCount = 1;
    writes[3].pBufferInfo = &bufferInfo;

    device_->getDevice().updateDescriptorSets(writes, {});
}

VulkanPipeline::MaterialDescriptorSlot& VulkanPipeline::acquireDescriptorSlot(
    std::vector<MaterialDescriptorSlot>& slots,
    vk::DescriptorSetLayout layout,
    uint32_t& imageIndex)
{
    const uint32_t slotCount = resources_->getFrameSlotCount();
    if (imageIndex >= slotCount) {
        imageIndex = 0;
    }

    if (slots.size() != slotCount) {
        slots.clear();
        slots.resize(slotCount);
    }

    MaterialDescriptorSlot& slot = slots[imageIndex];
    if (!slot.set) {
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = *descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        auto sets = device_->getDevice().allocateDescriptorSets(allocInfo);
        slot.set = std::make_unique<vk::raii::DescriptorSet>(std::move(sets.front()));
        slot.written = false;
    }

    return slot;
}

vk::DescriptorSet VulkanPipeline::getOrCreateMaterialDescriptorSet(const Material* material, uint32_t imageIndex) {
    MaterialDescriptorSlot& slot = acquireDescriptorSlot(
        materialDescriptorSets_[material], *materialDescriptorSetLayout_, imageIndex);

    const uint32_t revision = material ? material->getRevision() : 0;
    if (!slot.written || slot.appliedRevision != revision) {
        writeMaterialDescriptorSet(**slot.set, material, imageIndex);
        slot.appliedRevision = revision;
        slot.written = true;
    }

    return **slot.set;
}

ShaderMaterialUniforms VulkanPipeline::makeShaderMaterialUniforms(const Material& material) {
    ShaderMaterialUniforms uniforms{};
    uniforms.baseColor = glm::vec4(material.getColor(), 1.0f);
    const auto& customTextures = material.getCustomTextureUniforms();
    uniforms.textureFlags = glm::vec4(
        material.hasDiffuseTexture() ? 1.0f : 0.0f,
        customTextures.size() > 0 && !customTextures[0].second.empty() ? 1.0f : 0.0f,
        customTextures.size() > 1 && !customTextures[1].second.empty() ? 1.0f : 0.0f,
        customTextures.size() > 2 && !customTextures[2].second.empty() ? 1.0f : 0.0f);
    return uniforms;
}

void VulkanPipeline::writeShaderMaterialDescriptorSet(vk::DescriptorSet set, const Material* material, uint32_t imageIndex) {
    if (material) {
        resources_->ensureShaderMaterialUniformBuffer(material, imageIndex, makeShaderMaterialUniforms(*material));
    }

    const auto& defaultTexture = resources_->getOrCreateTexture("");
    const auto& diffuseTexture = (material && material->hasDiffuseTexture())
        ? resources_->getOrCreateTexture(material->getDiffuseTexturePath())
        : defaultTexture;

    auto resolveCustomTexture = [&](size_t index) -> const VulkanResources::VulkanTexture& {
        if (!material) {
            return defaultTexture;
        }
        const auto& customTextures = material->getCustomTextureUniforms();
        if (index >= customTextures.size() || customTextures[index].second.empty()) {
            return defaultTexture;
        }
        return resources_->getOrCreateTexture(customTextures[index].second);
    };

    const auto& customTexture0 = resolveCustomTexture(0);
    const auto& customTexture1 = resolveCustomTexture(1);
    const auto& customTexture2 = resolveCustomTexture(2);

    vk::DescriptorImageInfo diffuseInfo{};
    diffuseInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    diffuseInfo.imageView = *diffuseTexture.view;
    diffuseInfo.sampler = *diffuseTexture.sampler;

    vk::DescriptorImageInfo custom0Info{};
    custom0Info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    custom0Info.imageView = *customTexture0.view;
    custom0Info.sampler = *customTexture0.sampler;

    vk::DescriptorImageInfo custom1Info{};
    custom1Info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    custom1Info.imageView = *customTexture1.view;
    custom1Info.sampler = *customTexture1.sampler;

    vk::DescriptorImageInfo custom2Info{};
    custom2Info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    custom2Info.imageView = *customTexture2.view;
    custom2Info.sampler = *customTexture2.sampler;

    vk::DescriptorBufferInfo bufferInfo = resources_->getShaderMaterialDescriptorBufferInfo(material, imageIndex);

    std::array<vk::WriteDescriptorSet, 5> writes{};

    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &diffuseInfo;

    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &custom0Info;

    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &custom1Info;

    writes[3].dstSet = set;
    writes[3].dstBinding = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &custom2Info;

    writes[4].dstSet = set;
    writes[4].dstBinding = 4;
    writes[4].dstArrayElement = 0;
    writes[4].descriptorType = vk::DescriptorType::eUniformBuffer;
    writes[4].descriptorCount = 1;
    writes[4].pBufferInfo = &bufferInfo;

    device_->getDevice().updateDescriptorSets(writes, {});
}

vk::DescriptorSet VulkanPipeline::getOrCreateShaderMaterialDescriptorSet(const Material* material, uint32_t imageIndex) {
    MaterialDescriptorSlot& slot = acquireDescriptorSlot(
        shaderMaterialDescriptorSets_[material], *shaderMaterialDescriptorSetLayout_, imageIndex);

    const uint32_t revision = material ? material->getRevision() : 0;
    if (!slot.written || slot.appliedRevision != revision) {
        writeShaderMaterialDescriptorSet(**slot.set, material, imageIndex);
        slot.appliedRevision = revision;
        slot.written = true;
    }

    return **slot.set;
}

vk::DescriptorSet VulkanPipeline::getOrCreateCustomTextureDescriptorSet(const Material* material) {
    auto it = customTextureDescriptorSets_.find(material);
    if (it != customTextureDescriptorSets_.end()) {
        return *it->second;
    }

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *descriptorPool_;
    vk::DescriptorSetLayout layout = *customTextureDescriptorSetLayout_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    auto sets = device_->getDevice().allocateDescriptorSets(allocInfo);
    vk::raii::DescriptorSet set = std::move(sets.front());

    const auto& defaultTexture = resources_->getOrCreateTexture("");
    auto resolveCustomTexture = [&](size_t index) -> const VulkanResources::VulkanTexture& {
        if (!material) {
            return defaultTexture;
        }
        const auto& customTextures = material->getCustomTextureUniforms();
        if (index >= customTextures.size() || customTextures[index].second.empty()) {
            return defaultTexture;
        }
        return resources_->getOrCreateTexture(customTextures[index].second);
    };

    const auto& customTexture0 = resolveCustomTexture(0);
    const auto& customTexture1 = resolveCustomTexture(1);
    const auto& customTexture2 = resolveCustomTexture(2);

    vk::DescriptorImageInfo custom0Info{};
    custom0Info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    custom0Info.imageView = *customTexture0.view;
    custom0Info.sampler = *customTexture0.sampler;

    vk::DescriptorImageInfo custom1Info{};
    custom1Info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    custom1Info.imageView = *customTexture1.view;
    custom1Info.sampler = *customTexture1.sampler;

    vk::DescriptorImageInfo custom2Info{};
    custom2Info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    custom2Info.imageView = *customTexture2.view;
    custom2Info.sampler = *customTexture2.sampler;

    std::array<vk::WriteDescriptorSet, 3> writes{};
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &custom0Info;

    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &custom1Info;

    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &custom2Info;

    device_->getDevice().updateDescriptorSets(writes, {});
    customTextureDescriptorSets_.emplace(material, std::make_unique<vk::raii::DescriptorSet>(std::move(set)));
    return *customTextureDescriptorSets_[material].get();
}

vk::DescriptorSet VulkanPipeline::getOrUpdateEnvironmentDescriptorSet( const FrameEnvironment& env, const VulkanResources::VulkanTexture& cubemap){
    if (!environmentDescriptorSet_) {
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = *descriptorPool_;
        vk::DescriptorSetLayout layout = *environmentDescriptorSetLayout_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        auto sets = device_->getDevice().allocateDescriptorSets(allocInfo);
        environmentDescriptorSet_ = std::make_unique<vk::raii::DescriptorSet>(std::move(sets.front()));
        currentEnvironmentKey_ = nullptr;
    }

    if (env.cacheKey != currentEnvironmentKey_) {
        vk::DescriptorImageInfo cubemapInfo{};
        cubemapInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        cubemapInfo.imageView = *cubemap.view;
        cubemapInfo.sampler = *cubemap.sampler;

        vk::WriteDescriptorSet write{};
        write.dstSet = **environmentDescriptorSet_;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.descriptorCount = 1;
        write.pImageInfo = &cubemapInfo;

        device_->getDevice().updateDescriptorSets({write}, {});
        currentEnvironmentKey_ = env.cacheKey;
    }

    return **environmentDescriptorSet_;
}

void VulkanPipeline::writeTextDescriptorSet(vk::DescriptorSet set, const TextMaterial* material, const VulkanResources::VulkanTexture& atlasTexture, uint32_t imageIndex) {
    if (material) {
        TextMaterialUniforms mu{};
        mu.color = material->getColor();
        resources_->ensureTextMaterialUniformBuffer(material, imageIndex, mu);
    }

    vk::DescriptorImageInfo diffuseInfo{};
    diffuseInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    diffuseInfo.imageView = *atlasTexture.view;
    diffuseInfo.sampler = *atlasTexture.sampler;

    vk::DescriptorBufferInfo bufferInfo = resources_->getTextMaterialDescriptorBufferInfo(material, imageIndex);

    std::array<vk::WriteDescriptorSet, 2> writes{};

    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &diffuseInfo;

    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = vk::DescriptorType::eUniformBuffer;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &bufferInfo;

    device_->getDevice().updateDescriptorSets(writes, {});
}

vk::DescriptorSet VulkanPipeline::getOrCreateTextDescriptorSet(const TextMaterial* material, const VulkanResources::VulkanTexture& atlasTexture, uint32_t imageIndex){
    MaterialDescriptorSlot& slot = acquireDescriptorSlot(
        textMaterialDescriptorSets_[material], *textDescriptorSetLayout_, imageIndex);

    const uint32_t revision = material ? material->getRevision() : 0;
    if (!slot.written || slot.appliedRevision != revision) {
        writeTextDescriptorSet(**slot.set, material, atlasTexture, imageIndex);
        slot.appliedRevision = revision;
        slot.written = true;
    }

    return **slot.set;
}

void VulkanPipeline::clearSceneDescriptorCaches() {
    materialDescriptorSets_.clear();
    shaderMaterialDescriptorSets_.clear();
    customTextureDescriptorSets_.clear();
    textMaterialDescriptorSets_.clear();
}

void VulkanPipeline::recreateDescriptorSets() {
    shaderMaterialDescriptorSets_.clear();
    customTextureDescriptorSets_.clear();
    createDescriptorPoolAndSets();
}

bool VulkanPipeline::shaderSpvExists(const std::string& path) {
    return AssetPaths::exists(AssetPaths::resolve(path));
}

std::string VulkanPipeline::makeCustomPipelineCacheKey(const Material* material) {
    if (!material) {
        return "";
    }
    return material->getVulkanShaderPipelineKey() + "|" +
           std::to_string(static_cast<int>(material->getBlendMode())) + "|" +
           (material->getDepthWrite() ? "1" : "0") + "|" +
           (material->getDoubleSided() ? "1" : "0");
}

vk::PipelineColorBlendAttachmentState VulkanPipeline::makeBlendAttachment(BlendMode blendMode) {
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    switch (blendMode) {
        case BlendMode::Alpha:
            colorBlendAttachment.blendEnable = vk::True;
            colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
            break;
        case BlendMode::Additive:
            colorBlendAttachment.blendEnable = vk::True;
            colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOne;
            colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
            colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
            break;
        case BlendMode::Opaque:
        default:
            colorBlendAttachment.blendEnable = vk::False;
            break;
    }

    return colorBlendAttachment;
}

VulkanPipeline::CachedShaderPipeline VulkanPipeline::createShaderGraphicsPipeline(
    const std::string& vertSpvPath,
    const std::string& fragSpvPath,
    BlendMode blendMode,
    bool depthWrite,
    bool cullEnabled,
    uint32_t pushConstantSize,
    vk::ShaderStageFlags pushConstantStages,
    std::span<const vk::VertexInputAttributeDescription> vertexAttributes,
    bool useLitDescriptorLayout)
{
    vk::raii::ShaderModule vertexShaderModule = createShaderModule(readFile(vertSpvPath));
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(readFile(fragSpvPath));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = getMeshVertexBindingDescription();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = cullEnabled ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = vk::True;
    depthStencil.depthWriteEnable = depthWrite ? vk::True : vk::False;
    depthStencil.depthCompareOp = depthWrite ? vk::CompareOp::eLess : vk::CompareOp::eLessOrEqual;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment = makeBlendAttachment(blendMode);
    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PushConstantRange pushConstant{};
    pushConstant.stageFlags = pushConstantStages;
    pushConstant.offset = 0;
    pushConstant.size = pushConstantSize;

    std::array<vk::DescriptorSetLayout, 5> customLitSetLayouts = {
        *frameDescriptorSetLayout_,
        *materialDescriptorSetLayout_,
        *environmentDescriptorSetLayout_,
        *animationDescriptorSetLayout_,
        *customTextureDescriptorSetLayout_
    };
    std::array<vk::DescriptorSetLayout, 2> beamSetLayouts = {
        *frameDescriptorSetLayout_,
        *shaderMaterialDescriptorSetLayout_
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    if (useLitDescriptorLayout) {
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(customLitSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = customLitSetLayouts.data();
    } else {
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(beamSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = beamSetLayouts.data();
    }
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    vk::raii::PipelineLayout layout = vk::raii::PipelineLayout(device_->getDevice(), pipelineLayoutInfo);

    vk::Format depthFormat = resources_->findDepthFormat();
    vk::Format colorAttachmentFormat = swapChain_->getSurfaceFormat().format;

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pDepthStencilState = &depthStencil;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = layout;
    pipelineCreateInfo.renderPass = nullptr;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
        pipelineCreateInfo,
        pipelineRenderingInfo
    };

    CachedShaderPipeline result{};
    result.layout = std::move(layout);
    result.pipeline = vk::raii::Pipeline(
        device_->getDevice(),
        nullptr,
        pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    return result;
}

void VulkanPipeline::createBeamPipeline() {
    const auto shaderMaterialAttributes = getShaderMaterialVertexAttributeDescriptions();
    CachedShaderPipeline beamCached = createShaderGraphicsPipeline(
        kBeamVertexShaderSpvPath,
        kBeamFragmentShaderSpvPath,
        BlendMode::Additive,
        false,
        false,
        sizeof(BeamPushConstants),
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        shaderMaterialAttributes,
        false);
    beamPipeline_ = std::move(beamCached.pipeline);
    beamPipelineLayout_ = std::move(beamCached.layout);
}

VulkanPipeline::CachedShaderPipeline& VulkanPipeline::getOrCreateCustomShaderPipeline(const Material* material) {
    const std::string cacheKey = makeCustomPipelineCacheKey(material);
    auto it = customShaderPipelineCache_.find(cacheKey);
    if (it != customShaderPipelineCache_.end()) {
        return it->second;
    }

    const auto meshAttributes = getMeshVertexAttributeDescriptions();
    const bool cullEnabled = material ? !material->getDoubleSided() : true;
    CachedShaderPipeline cached = createShaderGraphicsPipeline(
        material->getVulkanVertexSpvPath(),
        material->getVulkanFragmentSpvPath(),
        material->getBlendMode(),
        material->getDepthWrite(),
        cullEnabled,
        sizeof(PushConstants),
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        meshAttributes,
        true);

    auto [insertIt, inserted] = customShaderPipelineCache_.emplace(cacheKey, std::move(cached));
    return insertIt->second;
}

const VulkanPipeline::CachedShaderPipeline* VulkanPipeline::getCustomShaderPipeline(const Material* material) {
    if (!material || material->getVulkanShaderPipelineKind() != VulkanShaderPipelineKind::Custom) {
        return nullptr;
    }

    const std::string vertPath = material->getVulkanVertexSpvPath();
    const std::string fragPath = material->getVulkanFragmentSpvPath();
    if (!shaderSpvExists(vertPath) || !shaderSpvExists(fragPath)) {
        static std::unordered_set<std::string> warnedMissingShaders;
        const std::string cacheKey = makeCustomPipelineCacheKey(material);
        if (warnedMissingShaders.insert(cacheKey).second) {
            std::cerr << "Vulkan: missing SPIR-V for custom material shader ("
                      << vertPath << ", " << fragPath << "). Draw skipped." << std::endl;
        }
        return nullptr;
    }

    CachedShaderPipeline& cached = getOrCreateCustomShaderPipeline(material);
    return &cached;
}

//WILL NEED REWORKS to not have hardcoded path
void VulkanPipeline::createGraphicsPipeline() {
    vk::raii::ShaderModule vertexShaderModule = createShaderModule(readFile(kDefaultVertexShaderSpvPath));
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(readFile(kDefaultFragmentShaderSpvPath));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = getMeshVertexBindingDescription();
    auto attributeDescriptions = getMeshVertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    // Current dynamic-rendering path targets 1-sample swapchain/depth attachments directly
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PushConstantRange pushConstant{};
    pushConstant.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstants);

    std::array<const vk::DescriptorSetLayout, 4> setLayouts = {
        *frameDescriptorSetLayout_,
        *materialDescriptorSetLayout_,
        *environmentDescriptorSetLayout_,
        *animationDescriptorSetLayout_
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    pipelineLayout_ = vk::raii::PipelineLayout(device_->getDevice(), pipelineLayoutInfo);

    vk::Format depthFormat = resources_->findDepthFormat();
    vk::Format colorAttachmentFormat = swapChain_->getSurfaceFormat().format;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

    const BlendMode blendModes[] = {
        BlendMode::Opaque,
        BlendMode::Alpha,
        BlendMode::Additive
    };
    const bool depthWriteOptions[] = { true, false };
    const bool cullOptions[] = { true, false };

    defaultLitPipelineCache_.clear();
    for (BlendMode blendMode : blendModes) {
        for (bool depthWrite : depthWriteOptions) {
            for (bool cullEnabled : cullOptions) {
                rasterizer.cullMode = cullEnabled ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eNone;

                vk::PipelineDepthStencilStateCreateInfo depthStencil{};
                depthStencil.depthTestEnable = vk::True;
                depthStencil.depthWriteEnable = depthWrite ? vk::True : vk::False;
                depthStencil.depthCompareOp = depthWrite ? vk::CompareOp::eLess : vk::CompareOp::eLessOrEqual;
                depthStencil.depthBoundsTestEnable = vk::False;
                depthStencil.stencilTestEnable = vk::False;

                vk::PipelineColorBlendAttachmentState colorBlendAttachment = makeBlendAttachment(blendMode);
                vk::PipelineColorBlendStateCreateInfo colorBlending{};
                colorBlending.logicOpEnable = vk::False;
                colorBlending.logicOp = vk::LogicOp::eCopy;
                colorBlending.attachmentCount = 1;
                colorBlending.pAttachments = &colorBlendAttachment;

                vk::GraphicsPipelineCreateInfo pipelineCreateInfo{};
                pipelineCreateInfo.stageCount = 2;
                pipelineCreateInfo.pStages = shaderStages;
                pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
                pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
                pipelineCreateInfo.pViewportState = &viewportState;
                pipelineCreateInfo.pRasterizationState = &rasterizer;
                pipelineCreateInfo.pMultisampleState = &multisampling;
                pipelineCreateInfo.pDepthStencilState = &depthStencil;
                pipelineCreateInfo.pColorBlendState = &colorBlending;
                pipelineCreateInfo.pDynamicState = &dynamicState;
                pipelineCreateInfo.layout = pipelineLayout_;
                pipelineCreateInfo.renderPass = nullptr;

                vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
                    pipelineCreateInfo,
                    pipelineRenderingInfo
                };

                const uint32_t key = makeDefaultLitPipelineKey(blendMode, depthWrite, cullEnabled);
                defaultLitPipelineCache_.emplace(
                    key,
                    vk::raii::Pipeline(
                        device_->getDevice(),
                        nullptr,
                        pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()));
            }
        }
    }

    // Keep opaque + depth-write + cull as the default handle for call sites that don't pass blend state
    const uint32_t defaultKey = makeDefaultLitPipelineKey(BlendMode::Opaque, true, true);
    graphicsPipeline_ = std::move(defaultLitPipelineCache_.at(defaultKey));
    defaultLitPipelineCache_.erase(defaultKey);
}

void VulkanPipeline::createInstancedGraphicsPipeline() {
    instancedLitPipelineCache_.clear();

    if (!shaderSpvExists(kDefaultInstancedVertexShaderSpvPath)) {
        std::cerr << "Vulkan: " << kDefaultInstancedVertexShaderSpvPath << " missing, GPU instancing disabled (falling back to per-entity draws)." << std::endl;
        return;
    }

    vk::raii::ShaderModule vertexShaderModule = createShaderModule(readFile(kDefaultInstancedVertexShaderSpvPath));
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(readFile(kDefaultFragmentShaderSpvPath));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescriptions = getMeshInstancedVertexBindingDescriptions();
    auto attributeDescriptions = getMeshInstancedVertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::Format depthFormat = resources_->findDepthFormat();
    vk::Format colorAttachmentFormat = swapChain_->getSurfaceFormat().format;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

    const bool depthWriteOptions[] = { true, false };
    const bool cullOptions[] = { true, false };

    for (bool depthWrite : depthWriteOptions) {
        for (bool cullEnabled : cullOptions) {
            rasterizer.cullMode = cullEnabled ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eNone;

            vk::PipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.depthTestEnable = vk::True;
            depthStencil.depthWriteEnable = depthWrite ? vk::True : vk::False;
            depthStencil.depthCompareOp = depthWrite ? vk::CompareOp::eLess : vk::CompareOp::eLessOrEqual;
            depthStencil.depthBoundsTestEnable = vk::False;
            depthStencil.stencilTestEnable = vk::False;

            vk::PipelineColorBlendAttachmentState colorBlendAttachment = makeBlendAttachment(BlendMode::Opaque);
            vk::PipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.logicOpEnable = vk::False;
            colorBlending.logicOp = vk::LogicOp::eCopy;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;

            vk::GraphicsPipelineCreateInfo pipelineCreateInfo{};
            pipelineCreateInfo.stageCount = 2;
            pipelineCreateInfo.pStages = shaderStages;
            pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
            pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
            pipelineCreateInfo.pViewportState = &viewportState;
            pipelineCreateInfo.pRasterizationState = &rasterizer;
            pipelineCreateInfo.pMultisampleState = &multisampling;
            pipelineCreateInfo.pDepthStencilState = &depthStencil;
            pipelineCreateInfo.pColorBlendState = &colorBlending;
            pipelineCreateInfo.pDynamicState = &dynamicState;
            pipelineCreateInfo.layout = pipelineLayout_;
            pipelineCreateInfo.renderPass = nullptr;

            vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
                pipelineCreateInfo,
                pipelineRenderingInfo
            };

            const uint32_t key = makeDefaultLitPipelineKey(BlendMode::Opaque, depthWrite, cullEnabled);
            instancedLitPipelineCache_.emplace(
                key,
                vk::raii::Pipeline( device_->getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>())
            );
        }
    }
}

vk::raii::Pipeline* VulkanPipeline::getInstancedGraphicsPipeline(bool depthWrite, bool cullEnabled) {
    const uint32_t key = makeDefaultLitPipelineKey(BlendMode::Opaque, depthWrite, cullEnabled);
    auto it = instancedLitPipelineCache_.find(key);
    return (it == instancedLitPipelineCache_.end()) ? nullptr : &it->second;
}

bool VulkanPipeline::hasInstancedPipelines() const {
    return !instancedLitPipelineCache_.empty();
}

uint32_t VulkanPipeline::makeDefaultLitPipelineKey(BlendMode blendMode, bool depthWrite, bool cullEnabled) {
    return (static_cast<uint32_t>(blendMode) & 0xFFu)
         | (depthWrite ? 0x100u : 0u)
         | (cullEnabled ? 0x200u : 0u);
}

void VulkanPipeline::createTextPipeline(bool depthTestEnable, vk::raii::Pipeline& outPipeline){
    vk::raii::ShaderModule vertexShaderModule = createShaderModule(readFile(kTextVertexShaderSpvPath));
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(readFile(kTextFragmentShaderSpvPath));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = getTextVertexBindingDescription();
    auto attributeDescriptions = getTextVertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    // Current dynamic-rendering path targets 1-sample swapchain/depth attachments directly.
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = depthTestEnable ? vk::True : vk::False;
    depthStencil.depthWriteEnable = vk::False;
    depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = vk::True;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    
    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;                                                

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();                                            

    vk::PushConstantRange pushConstant{};
    pushConstant.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstants);

    std::array<const vk::DescriptorSetLayout, 2> setLayouts = {*frameDescriptorSetLayout_, *textDescriptorSetLayout_};
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    textPipelineLayout_ = vk::raii::PipelineLayout(device_->getDevice(), pipelineLayoutInfo);

    vk::Format depthFormat = resources_->findDepthFormat();
    vk::Format colorAttachmentFormat = swapChain_->getSurfaceFormat().format;

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pDepthStencilState = &depthStencil;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = textPipelineLayout_;
    pipelineCreateInfo.renderPass = nullptr;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
        pipelineCreateInfo,
        pipelineRenderingInfo
    };

    outPipeline = vk::raii::Pipeline(device_->getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void VulkanPipeline::createShadowPipeline() {
    if (!resources_->hasShadowAtlas()) {
        return;
    }

    vk::raii::ShaderModule vertexShaderModule = createShaderModule(readFile(kShadowVertexShaderSpvPath));
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(readFile(kShadowFragmentShaderSpvPath));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Same vertex layout as the lit pass so casters reuse their uploaded meshes
    auto bindingDescription = getMeshVertexBindingDescription();
    auto attributeDescriptions = getMeshVertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = vk::True;
    depthStencil.depthWriteEnable = vk::True;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable = vk::False;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.attachmentCount = 0;

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.colorAttachmentCount = 0;
    pipelineRenderingInfo.depthAttachmentFormat = resources_->getShadowAtlasFormat();

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pDepthStencilState = &depthStencil;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = pipelineLayout_;
    pipelineCreateInfo.renderPass = nullptr;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
        pipelineCreateInfo,
        pipelineRenderingInfo
    };

    shadowPipeline_ = vk::raii::Pipeline(
        device_->getDevice(),
        nullptr,
        pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

vk::raii::Pipeline& VulkanPipeline::getShadowPipeline() {
    return shadowPipeline_;
}

void VulkanPipeline::createSkyboxPipeline() {
    vk::raii::ShaderModule vertexShaderModule = createShaderModule(readFile(kSkyboxVertexShaderSpvPath));
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(readFile(kSkyboxFragmentShaderSpvPath));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";
    
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = getMeshVertexBindingDescription();
    const auto attributeDescriptions = getSkyboxVertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = vk::True;
    depthStencil.depthWriteEnable = vk::True;
    depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    std::array<const vk::DescriptorSetLayout, 2> setLayouts = {
        *frameDescriptorSetLayout_,
        *environmentDescriptorSetLayout_
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();

    skyboxPipelineLayout_ = vk::raii::PipelineLayout(device_->getDevice(), pipelineLayoutInfo);

    vk::Format depthFormat = resources_->findDepthFormat();
    vk::Format colorAttachmentFormat = swapChain_->getSurfaceFormat().format;

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pDepthStencilState = &depthStencil;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = skyboxPipelineLayout_;
    pipelineCreateInfo.renderPass = nullptr;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
        pipelineCreateInfo,
        pipelineRenderingInfo
    };

    skyboxPipeline_ = vk::raii::Pipeline(device_->getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void VulkanPipeline::createParticleGraphicsPipeline() {
    vk::raii::ShaderModule vertexShaderModule = createShaderModule(readFile(kDefaultVertexShaderSpvPath));
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(readFile(kDefaultFragmentShaderSpvPath));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = Particle::getBindingDescription();
    auto attributeDescriptions = Particle::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();                           

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::ePointList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    // Current dynamic-rendering path targets 1-sample swapchain/depth attachments directly.
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = vk::False;
    depthStencil.depthWriteEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = vk::True;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 0;
    particlePipelineLayout_ = vk::raii::PipelineLayout(device_->getDevice(), pipelineLayoutInfo);
    
    vk::Format depthFormat = resources_->findDepthFormat();
    vk::Format colorAttachmentFormat = swapChain_->getSurfaceFormat().format;

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = particlePipelineLayout_;
    pipelineInfo.renderPass = nullptr;

    vk::PipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,vk::PipelineRenderingCreateInfo > pipelineCreateInfoChain{
        pipelineInfo,
        renderingInfo
    };

    particlePipeline_ = vk::raii::Pipeline(device_->getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

vk::raii::ShaderModule VulkanPipeline::createShaderModule(const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    return vk::raii::ShaderModule{device_->getDevice(), createInfo};
}

vk::VertexInputBindingDescription VulkanPipeline::getMeshVertexBindingDescription() {
    vk::VertexInputBindingDescription b{};
    b.binding = 0;
    b.stride = sizeof(GameEngine::Vertex);
    b.inputRate = vk::VertexInputRate::eVertex;
    return b;
}

std::array<vk::VertexInputAttributeDescription, 6> VulkanPipeline::getMeshVertexAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 6> a{};

    a[0].binding = 0; a[0].location = 0; a[0].format = vk::Format::eR32G32B32Sfloat;    a[0].offset = offsetof(GameEngine::Vertex, position);
    a[1].binding = 0; a[1].location = 1; a[1].format = vk::Format::eR32G32B32Sfloat;    a[1].offset = offsetof(GameEngine::Vertex, normal);
    a[2].binding = 0; a[2].location = 2; a[2].format = vk::Format::eR32G32Sfloat;       a[2].offset = offsetof(GameEngine::Vertex, texCoords);
    a[3].binding = 0; a[3].location = 3; a[3].format = vk::Format::eR32G32B32Sfloat;    a[3].offset = offsetof(GameEngine::Vertex, tangent);
    a[4].binding = 0; a[4].location = 4; a[4].format = vk::Format::eR32G32B32A32Sfloat; a[4].offset = offsetof(GameEngine::Vertex, boneWeights);
    a[5].binding = 0; a[5].location = 5; a[5].format = vk::Format::eR32G32B32A32Sfloat; a[5].offset = offsetof(GameEngine::Vertex, boneIndices);

    return a;
}

std::array<vk::VertexInputBindingDescription, 2> VulkanPipeline::getMeshInstancedVertexBindingDescriptions() {
    std::array<vk::VertexInputBindingDescription, 2> b{};

    b[0] = getMeshVertexBindingDescription();

    b[1].binding = 1;
    b[1].stride = sizeof(GameEngine::InstanceData);
    b[1].inputRate = vk::VertexInputRate::eInstance;

    return b;
}

std::array<vk::VertexInputAttributeDescription, 11> VulkanPipeline::getMeshInstancedVertexAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 11> a{};

    const auto meshAttributes = getMeshVertexAttributeDescriptions();
    for (size_t i = 0; i < 4; ++i) {
        a[i] = meshAttributes[i];
    }

    for (uint32_t column = 0; column < 4; ++column) {
        auto& attribute = a[4 + column];
        attribute.binding = 1;
        attribute.location = 6 + column;
        attribute.format = vk::Format::eR32G32B32A32Sfloat;
        attribute.offset = static_cast<uint32_t>(offsetof(GameEngine::InstanceData, modelMatrix) + column * sizeof(glm::vec4));
    }

    for (uint32_t column = 0; column < 3; ++column) {
        auto& attribute = a[8 + column];
        attribute.binding = 1;
        attribute.location = 10 + column;
        attribute.format = vk::Format::eR32G32B32A32Sfloat;
        attribute.offset = static_cast<uint32_t>(offsetof(GameEngine::InstanceData, normalMatrix) + column * sizeof(glm::vec4));
    }

    return a;
}

std::array<vk::VertexInputAttributeDescription, 1> VulkanPipeline::getSkyboxVertexAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 1> a{};
    a[0].binding = 0;
    a[0].location = 0;
    a[0].format = vk::Format::eR32G32B32Sfloat;
    a[0].offset = offsetof(GameEngine::Vertex, position);
    return a;
}

std::array<vk::VertexInputAttributeDescription, 2> VulkanPipeline::getShaderMaterialVertexAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> a{};
    a[0].binding = 0;
    a[0].location = 0;
    a[0].format = vk::Format::eR32G32B32Sfloat;
    a[0].offset = offsetof(GameEngine::Vertex, position);
    a[1].binding = 0;
    a[1].location = 2;
    a[1].format = vk::Format::eR32G32Sfloat;
    a[1].offset = offsetof(GameEngine::Vertex, texCoords);
    return a;
}

vk::VertexInputBindingDescription VulkanPipeline::getTextVertexBindingDescription(){
    vk::VertexInputBindingDescription b{};
    b.binding = 0;
    b.stride = sizeof(GameEngine::TextVertex);
    b.inputRate = vk::VertexInputRate::eVertex;
    return b;
}

std::array<vk::VertexInputAttributeDescription, 2> VulkanPipeline::getTextVertexAttributeDescriptions()
{
    std::array<vk::VertexInputAttributeDescription, 2> a{};

    // position
    a[0].binding = 0;
    a[0].location = 0;
    a[0].format = vk::Format::eR32G32B32Sfloat;
    a[0].offset = offsetof(GameEngine::TextVertex, position);

    // texcoord
    a[1].binding = 0;
    a[1].location = 1;
    a[1].format = vk::Format::eR32G32Sfloat;
    a[1].offset = offsetof(GameEngine::TextVertex, texCoord);

    return a;
}

vk::raii::PipelineLayout& VulkanPipeline::getGraphicsPipelineLayout(){
    return pipelineLayout_;
}

vk::raii::Pipeline& VulkanPipeline::getGraphicsPipeline(){
    return graphicsPipeline_;
}

vk::raii::Pipeline& VulkanPipeline::getGraphicsPipeline(BlendMode blendMode, bool depthWrite, bool cullEnabled) {
    if (blendMode == BlendMode::Opaque && depthWrite && cullEnabled) {
        return graphicsPipeline_;
    }
    const uint32_t key = makeDefaultLitPipelineKey(blendMode, depthWrite, cullEnabled);
    auto it = defaultLitPipelineCache_.find(key);
    if (it != defaultLitPipelineCache_.end()) {
        return it->second;
    }
    return graphicsPipeline_;
}

vk::raii::PipelineLayout& VulkanPipeline::getTextPipelineLayout(){
    return textPipelineLayout_;
}

vk::raii::Pipeline& VulkanPipeline::getTextPipeline(){
    return textPipeline_;
}

vk::raii::Pipeline& VulkanPipeline::getWorldTextPipeline(){
    return worldTextPipeline_;
}

vk::raii::PipelineLayout& VulkanPipeline::getSkyboxPipelineLayout(){
    return skyboxPipelineLayout_;
}

vk::raii::Pipeline& VulkanPipeline::getSkyboxPipeline(){
    return skyboxPipeline_;
}

vk::raii::PipelineLayout& VulkanPipeline::getBeamPipelineLayout(){
    return beamPipelineLayout_;
}

vk::raii::Pipeline& VulkanPipeline::getBeamPipeline(){
    return beamPipeline_;
}

vk::DescriptorSet VulkanPipeline::getDescriptorSet(uint32_t index) {
    if (index >= descriptorSets_.size()) throw std::out_of_range("descriptor set index out of range");
    return *descriptorSets_[index];
}

std::vector<char> VulkanPipeline::readFile(const std::string& filename) {
    const std::string resolvedPath = AssetPaths::resolve(filename);

    std::ifstream file(resolvedPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + resolvedPath);
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

}