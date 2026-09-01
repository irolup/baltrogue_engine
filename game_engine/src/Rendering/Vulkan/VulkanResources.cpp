#include "Rendering/Vulkan/VulkanResources.h"
#include "Core/AssetPaths.h"
#include "Rendering/Material.h"
#include "../../vendor/tinygltf/stb_image.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace GameEngine {

void VulkanResources::create(VulkanDevice& device, VulkanSwapChain& swapChain){
    device_ = &device;
    swapChain_ = &swapChain;
    createCommandPool();
    createDepthResources();
    createUniformBuffers(sizeof(PerFrameUniforms));
    createAnimationUniformBuffers();
}

uint32_t VulkanResources::getFrameSlotCount() const {
    return swapChain_ ? static_cast<uint32_t>(swapChain_->getImages().size()) : 1;
}

void VulkanResources::ensureMaterialUniformBuffer(const Material* material, uint32_t imageIndex, const MaterialUniforms& data) {
    if (!material) return;

    const uint32_t slotCount = getFrameSlotCount();
    if (imageIndex >= slotCount) return;

    std::vector<MaterialBuffer>& buffers = materialUniformBuffers_[material];
    if (buffers.size() != slotCount) {
        buffers.clear();
        buffers.resize(slotCount);
    }

    const vk::DeviceSize dataSize = sizeof(MaterialUniforms);
    MaterialBuffer& materialBuffer = buffers[imageIndex];
    if (materialBuffer.size != dataSize) {
        createBuffer(
            dataSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            materialBuffer.buffer,
            materialBuffer.memory);
        materialBuffer.size = dataSize;
    }

    void* mapped = materialBuffer.memory.mapMemory(0, dataSize);
    memcpy(mapped, &data, static_cast<size_t>(dataSize));
    materialBuffer.memory.unmapMemory();
}

void VulkanResources::ensureShaderMaterialUniformBuffer(const Material* material, uint32_t imageIndex, const ShaderMaterialUniforms& data) {
    if (!material) return;

    const uint32_t slotCount = getFrameSlotCount();
    if (imageIndex >= slotCount) return;

    std::vector<ShaderMaterialBuffer>& buffers = shaderMaterialUniformBuffers_[material];
    if (buffers.size() != slotCount) {
        buffers.clear();
        buffers.resize(slotCount);
    }

    const vk::DeviceSize dataSize = sizeof(ShaderMaterialUniforms);
    ShaderMaterialBuffer& materialBuffer = buffers[imageIndex];
    if (materialBuffer.size != dataSize) {
        createBuffer(
            dataSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            materialBuffer.buffer,
            materialBuffer.memory);
        materialBuffer.size = dataSize;
    }

    void* mapped = materialBuffer.memory.mapMemory(0, dataSize);
    memcpy(mapped, &data, static_cast<size_t>(dataSize));
    materialBuffer.memory.unmapMemory();
}

void VulkanResources::createAnimationUniformBuffers() {
    const uint32_t imageCount = getFrameSlotCount();
    const uint32_t totalSlots = kMaxSkinnedDrawsPerFrame * imageCount;

    animationUniformBuffers_.resize(totalSlots);
    const vk::DeviceSize bufferSize = sizeof(AnimationUniforms);

    for (uint32_t slot = 0; slot < totalSlots; ++slot) {
        AnimationBuffer buffer{};
        createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            buffer.buffer,
            buffer.memory);
        buffer.size = bufferSize;
        animationUniformBuffers_[slot] = std::move(buffer);
    }
}

void VulkanResources::writeAnimationUniform(uint32_t slot, const std::vector<glm::mat4>& boneTransforms) {
    if (slot >= animationUniformBuffers_.size()) {
        return;
    }

    const size_t boneCount = std::min(boneTransforms.size(), static_cast<size_t>(kMaxBones));

    auto& buffer = animationUniformBuffers_[slot];
    void* mapped = buffer.memory.mapMemory(0, buffer.size);
    auto* dst = static_cast<AnimationUniforms*>(mapped);
    dst->numBones = static_cast<int32_t>(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        dst->boneMatrices[i] = boneTransforms[i];
    }
    buffer.memory.unmapMemory();
}

vk::DescriptorBufferInfo VulkanResources::getAnimationDescriptorBufferInfo(uint32_t slot) const {
    vk::DescriptorBufferInfo info{};
    if (slot < animationUniformBuffers_.size()) {
        const auto& buffer = animationUniformBuffers_[slot];
        info.buffer = *buffer.buffer;
        info.offset = 0;
        info.range = buffer.size;
    }
    return info;
}

void VulkanResources::ensureTextMaterialUniformBuffer(const TextMaterial* material, uint32_t imageIndex, const TextMaterialUniforms& data) {
    if (!material) return;

    const uint32_t slotCount = getFrameSlotCount();
    if (imageIndex >= slotCount) return;

    std::vector<TextMaterialBuffer>& buffers = textMaterialUniformBuffers_[material];
    if (buffers.size() != slotCount) {
        buffers.clear();
        buffers.resize(slotCount);
    }

    const vk::DeviceSize dataSize = sizeof(TextMaterialUniforms);
    TextMaterialBuffer& materialBuffer = buffers[imageIndex];
    if (materialBuffer.size != dataSize) {
        createBuffer( dataSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, materialBuffer.buffer, materialBuffer.memory); materialBuffer.size = dataSize;
    }

    void* mapped = materialBuffer.memory.mapMemory(0, dataSize);
    memcpy(mapped, &data, static_cast<size_t>(dataSize));
    materialBuffer.memory.unmapMemory();
}

vk::DescriptorBufferInfo VulkanResources::getMaterialDescriptorBufferInfo(const Material* material, uint32_t imageIndex) const {
    vk::DescriptorBufferInfo info{};
    if (!material) return info;
    auto it = materialUniformBuffers_.find(material);
    if (it == materialUniformBuffers_.end() || imageIndex >= it->second.size()) return info;
    const MaterialBuffer& materialBuffer = it->second[imageIndex];
    if (materialBuffer.size == 0) return info;
    info.buffer = *materialBuffer.buffer;
    info.offset = 0;
    info.range = materialBuffer.size;
    return info;
}

vk::DescriptorBufferInfo VulkanResources::getShaderMaterialDescriptorBufferInfo(const Material* material, uint32_t imageIndex) const {
    vk::DescriptorBufferInfo info{};
    if (!material) return info;
    auto it = shaderMaterialUniformBuffers_.find(material);
    if (it == shaderMaterialUniformBuffers_.end() || imageIndex >= it->second.size()) return info;
    const ShaderMaterialBuffer& materialBuffer = it->second[imageIndex];
    if (materialBuffer.size == 0) return info;
    info.buffer = *materialBuffer.buffer;
    info.offset = 0;
    info.range = materialBuffer.size;
    return info;
}

vk::DescriptorBufferInfo VulkanResources::getTextMaterialDescriptorBufferInfo(const TextMaterial* material, uint32_t imageIndex) const {
    vk::DescriptorBufferInfo info{};
    if (!material) return info;
    auto it = textMaterialUniformBuffers_.find(material);
    if (it == textMaterialUniformBuffers_.end() || imageIndex >= it->second.size()) return info;
    const TextMaterialBuffer& materialBuffer = it->second[imageIndex];
    if (materialBuffer.size == 0) return info;
    info.buffer = *materialBuffer.buffer;
    info.offset = 0;
    info.range = materialBuffer.size;
    return info;
}

void VulkanResources::createUniformBuffers(vk::DeviceSize size) {
    uniformBufferSize_ = size;
    size_t imageCount = swapChain_->getImages().size();
    uniformBuffers_.clear();
    uniformBuffersMemory_.clear();
    uniformBuffersMapped_.clear();
    uniformBuffers_.reserve(imageCount);
    uniformBuffersMemory_.reserve(imageCount);
    uniformBuffersMapped_.reserve(imageCount);

    for (size_t i = 0; i < imageCount; ++i) {
        vk::raii::Buffer buffer = nullptr;
        vk::raii::DeviceMemory memory = nullptr;
        createBuffer(size, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, memory);
        void* data = memory.mapMemory(0, size);
        memset(data, 0, static_cast<size_t>(size));
        uniformBuffers_.push_back(std::move(buffer));
        uniformBuffersMemory_.push_back(std::move(memory));
        uniformBuffersMapped_.push_back(data);
    }
}

void VulkanResources::writeUniformBuffer(uint32_t index, const void* data, vk::DeviceSize size) {
    if (index >= uniformBuffersMapped_.size() || size > uniformBufferSize_) {
        return;
    }
    memcpy(uniformBuffersMapped_[index], data, static_cast<size_t>(size));
}

vk::raii::Buffer& VulkanResources::getUniformBuffer(uint32_t index) {
    if (index >= uniformBuffers_.size()) throw std::out_of_range("uniform buffer index out of range");
    return uniformBuffers_[index];
}

vk::raii::DeviceMemory& VulkanResources::getUniformBufferMemory(uint32_t index) {
    if (index >= uniformBuffersMemory_.size()) throw std::out_of_range("uniform buffer memory index out of range");
    return uniformBuffersMemory_[index];
}

bool VulkanResources::ensureInstanceBuffer(uint32_t imageIndex, vk::DeviceSize size) {
    const uint32_t slotCount = getFrameSlotCount();
    if (imageIndex >= slotCount || size == 0) {
        return false;
    }

    if (instanceBuffers_.size() < slotCount) {
        instanceBuffers_.resize(slotCount);
    }

    InstanceBuffer& slot = instanceBuffers_[imageIndex];
    if (slot.mapped && slot.size >= size) {
        return true;
    }

    vk::DeviceSize newSize = slot.size > 0 ? slot.size : 1024;
    while (newSize < size) {
        newSize *= 2;
    }

    vk::raii::Buffer buffer = nullptr;
    vk::raii::DeviceMemory memory = nullptr;
    createBuffer(newSize, vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, memory);

    void* mapped = memory.mapMemory(0, newSize);
    if (!mapped) {
        return false;
    }

    slot.buffer = std::move(buffer);
    slot.memory = std::move(memory);
    slot.mapped = mapped;
    slot.size = newSize;
    return true;
}

bool VulkanResources::writeInstanceBuffer(uint32_t imageIndex, const void* data, vk::DeviceSize size) {
    if (!data || !ensureInstanceBuffer(imageIndex, size)) {
        return false;
    }
    memcpy(instanceBuffers_[imageIndex].mapped, data, static_cast<size_t>(size));
    return true;
}

vk::Buffer VulkanResources::getInstanceBuffer(uint32_t imageIndex) const {
    if (imageIndex >= instanceBuffers_.size() || !instanceBuffers_[imageIndex].mapped) {
        return nullptr;
    }
    return *instanceBuffers_[imageIndex].buffer;
}


void VulkanResources::createCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags =vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = device_->getQueueFamilyIndex();

    commandPool_ = vk::raii::CommandPool(device_->getDevice(), poolInfo);
}

void VulkanResources::ensureUploadResources() {
    if (uploadCommandBuffer_ != nullptr && uploadFence_ != nullptr) {
        return;
    }

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = *commandPool_;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;
    uploadCommandBuffer_ = std::move(device_->getDevice().allocateCommandBuffers(allocInfo).front());

    vk::FenceCreateInfo fenceInfo{};
    uploadFence_ = vk::raii::Fence(device_->getDevice(), fenceInfo);
}

vk::raii::CommandBuffer& VulkanResources::getUploadCommandBuffer() {
    ensureUploadResources();
    if (!uploadBatchActive_) {
        if (uploadInFlight_) {
            waitForUploads();
        }
        uploadCommandBuffer_.reset({});
        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        uploadCommandBuffer_.begin(beginInfo);
        uploadBatchActive_ = true;
    }
    return uploadCommandBuffer_;
}

void VulkanResources::flushUploads() {
    if (!uploadBatchActive_) {
        return;
    }

    uploadCommandBuffer_.end();
    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    vk::CommandBuffer commandBuffer = *uploadCommandBuffer_;
    submitInfo.pCommandBuffers = &commandBuffer;
    device_->getQueue().submit(submitInfo, *uploadFence_);
    uploadBatchActive_ = false;
    uploadInFlight_ = true;
}

void VulkanResources::waitForUploads() {
    if (uploadBatchActive_) {
        flushUploads();
    }
    if (!uploadInFlight_) {
        return;
    }

    (void)device_->getDevice().waitForFences({*uploadFence_}, VK_TRUE, UINT64_MAX);
    device_->getDevice().resetFences({*uploadFence_});
    uploadInFlight_ = false;
    uploadStagingRetention_.clear();
}

VulkanResources::RetainedStagingBuffer& VulkanResources::createRetainedStagingBuffer(vk::DeviceSize size) {
    uploadStagingRetention_.emplace_back();
    auto& staging = uploadStagingRetention_.back();
    createBuffer(
        size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        staging.buffer,
        staging.memory);
    return staging;
}

void VulkanResources::createDepthResources() {
    vk::Format depthFormat = findDepthFormat();
    createImage(swapChain_->getExtent().width, swapChain_->getExtent().height, 1, vk::SampleCountFlagBits::e1, depthFormat, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage_, depthImageMemory_);
    depthImageView_ = createImageView(depthImage_, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

    auto& commandBuffer = getUploadCommandBuffer();
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    barrier.image = depthImage_;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, {}, {}, barrier);
    waitForUploads();
}

bool VulkanResources::ensureShadowAtlas(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return false;
    }
    if (shadowAtlasWidth_ == width && shadowAtlasHeight_ == height) {
        return true;
    }

    device_->getDevice().waitIdle();

    shadowAtlasImageView_ = nullptr;
    shadowAtlasImage_ = nullptr;
    shadowAtlasMemory_ = nullptr;
    shadowAtlasWidth_ = 0;
    shadowAtlasHeight_ = 0;

    // 16 bits is plenty for a shadow map and halves the bandwidth of the pass
    shadowAtlasFormat_ = findSupportedFormat(
        {vk::Format::eD16Unorm, vk::Format::eD32Sfloat},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment | vk::FormatFeatureFlagBits::eSampledImage);

    createImage(width, height, 1, vk::SampleCountFlagBits::e1, shadowAtlasFormat_, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                vk::MemoryPropertyFlagBits::eDeviceLocal, shadowAtlasImage_, shadowAtlasMemory_);
    shadowAtlasImageView_ = createImageView(shadowAtlasImage_, shadowAtlasFormat_, vk::ImageAspectFlagBits::eDepth, 1);

    if (!*shadowAtlasSampler_) {
        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.magFilter = vk::Filter::eNearest;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.anisotropyEnable = vk::False;
        samplerInfo.compareEnable = vk::False;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        shadowAtlasSampler_ = vk::raii::Sampler(device_->getDevice(), samplerInfo);
    }

    auto& commandBuffer = getUploadCommandBuffer();
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    barrier.image = shadowAtlasImage_;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                  vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
    waitForUploads();

    shadowAtlasWidth_ = width;
    shadowAtlasHeight_ = height;
    return true;
}

vk::Format VulkanResources::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (const auto format : candidates) {
        vk::FormatProperties props = device_->getPhysicalDevice().getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

vk::Format VulkanResources::findDepthFormat() {
    return findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal,
                               vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

void VulkanResources::createTextureImage(std::string texturePath) {
    int texWidth, texHeight, texChannels;
    const std::string resolvedPath = resolveTexturePath(texturePath);
    stbi_uc* pixels = stbi_load(resolvedPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    mipLevels_ = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    vk::raii::Buffer stagingBuffer = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory );

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, mipLevels_, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage_, textureImageMemory_);
    transitionImageLayout(textureImage_, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels_);
    copyBufferToImage(stagingBuffer, textureImage_, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    //transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels);
    generateMipmaps(textureImage_, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels_);
    waitForUploads();
}

std::string VulkanResources::resolveTexturePath(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    const std::string resolved = AssetPaths::resolveTexture(path);
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(resolved, ec);
    return ec ? resolved : canonical.generic_string();
}

bool VulkanResources::isEmbeddedTextureKey(const std::string& key) {
    return key.rfind("embedded:", 0) == 0;
}

const VulkanResources::VulkanTexture& VulkanResources::getOrCreateTextureFromMemory(
    const uint8_t* data, uint32_t width, uint32_t height, int channels, const std::string& cacheKey)
{
    auto it = textureCache_.find(cacheKey);
    if (it != textureCache_.end()) {
        return it->second;
    }

    if (!data || width == 0 || height == 0 || channels < 1 || channels > 4) {
        throw std::runtime_error("invalid embedded texture data: " + cacheKey);
    }

    const vk::DeviceSize pixelCount = static_cast<vk::DeviceSize>(width) * height;
    const vk::DeviceSize rgbaSize = pixelCount * 4;
    std::vector<uint8_t> rgba(static_cast<size_t>(rgbaSize));

    if (channels == 4) {
        memcpy(rgba.data(), data, static_cast<size_t>(rgbaSize));
    } else if (channels == 3) {
        for (vk::DeviceSize i = 0; i < pixelCount; ++i) {
            rgba[static_cast<size_t>(i * 4 + 0)] = data[static_cast<size_t>(i * 3 + 0)];
            rgba[static_cast<size_t>(i * 4 + 1)] = data[static_cast<size_t>(i * 3 + 1)];
            rgba[static_cast<size_t>(i * 4 + 2)] = data[static_cast<size_t>(i * 3 + 2)];
            rgba[static_cast<size_t>(i * 4 + 3)] = 255;
        }
    } else {
        for (vk::DeviceSize i = 0; i < pixelCount; ++i) {
            const uint8_t gray = data[static_cast<size_t>(i)];
            rgba[static_cast<size_t>(i * 4 + 0)] = gray;
            rgba[static_cast<size_t>(i * 4 + 1)] = gray;
            rgba[static_cast<size_t>(i * 4 + 2)] = gray;
            rgba[static_cast<size_t>(i * 4 + 3)] = 255;
        }
    }

    VulkanTexture tex{};
    const int texWidth = static_cast<int>(width);
    const int texHeight = static_cast<int>(height);
    tex.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    auto& staging = createRetainedStagingBuffer(rgbaSize);
    void* stagingData = staging.memory.mapMemory(0, rgbaSize);
    memcpy(stagingData, rgba.data(), static_cast<size_t>(rgbaSize));
    staging.memory.unmapMemory();

    createImage(width, height, tex.mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                vk::MemoryPropertyFlagBits::eDeviceLocal, tex.image, tex.memory);
    transitionImageLayout(tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, tex.mipLevels);
    copyBufferToImage(staging.buffer, tex.image, width, height);
    generateMipmaps(tex.image, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, tex.mipLevels);

    tex.view = createImageView(tex.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, tex.mipLevels);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(tex.mipLevels);
    tex.sampler = vk::raii::Sampler(device_->getDevice(), samplerInfo);

    auto [insIt, ok] = textureCache_.try_emplace(cacheKey, std::move(tex));
    return insIt->second;
}

const VulkanResources::VulkanTexture& VulkanResources::getOrCreateTexture(const std::string& path) {
    if (path.empty()) {
        static const std::string fallbackKey = "__default_white_texture__";
        auto it = textureCache_.find(fallbackKey);
        if (it != textureCache_.end()) return it->second;

        VulkanTexture tex{};
        constexpr std::array<unsigned char, 4> whitePixel = {255, 255, 255, 255};
        vk::DeviceSize imageSize = whitePixel.size();
        tex.mipLevels = 1;

        auto& staging = createRetainedStagingBuffer(imageSize);
        void* data = staging.memory.mapMemory(0, imageSize);
        memcpy(data, whitePixel.data(), static_cast<size_t>(imageSize));
        staging.memory.unmapMemory();

        createImage(1, 1, tex.mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, tex.image, tex.memory);
        transitionImageLayout(tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, tex.mipLevels);
        copyBufferToImage(staging.buffer, tex.image, 1, 1);
        generateMipmaps(tex.image, vk::Format::eR8G8B8A8Srgb, 1, 1, tex.mipLevels);

        tex.view = createImageView(tex.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, tex.mipLevels);

        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(tex.mipLevels);
        tex.sampler = vk::raii::Sampler(device_->getDevice(), samplerInfo);

        auto [insIt, ok] = textureCache_.try_emplace(fallbackKey, std::move(tex));
        return insIt->second;
    }

    if (isEmbeddedTextureKey(path)) {
        auto embeddedIt = textureCache_.find(path);
        if (embeddedIt != textureCache_.end()) {
            return embeddedIt->second;
        }
        throw std::runtime_error("embedded texture not loaded: " + path);
    }

    const std::string resolvedPath = resolveTexturePath(path);

    auto it = textureCache_.find(resolvedPath);
    if (it != textureCache_.end()) return it->second;

    VulkanTexture tex{};

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(resolvedPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels && resolvedPath != path) {
        pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    }
    if (!pixels) {
        throw std::runtime_error("failed to load texture: " + resolvedPath + " (requested: " + path + ")");
    }
    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    tex.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    auto& staging = createRetainedStagingBuffer(imageSize);
    void* data = staging.memory.mapMemory(0, imageSize);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    staging.memory.unmapMemory();
    stbi_image_free(pixels);

    createImage(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), tex.mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, tex.image, tex.memory);
    transitionImageLayout(tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, tex.mipLevels);
    copyBufferToImage(staging.buffer, tex.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    generateMipmaps(tex.image, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, tex.mipLevels);

    tex.view = createImageView(tex.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, tex.mipLevels);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(tex.mipLevels);
    tex.sampler = vk::raii::Sampler(device_->getDevice(), samplerInfo);

    auto [insIt, ok] = textureCache_.try_emplace(resolvedPath, std::move(tex));
    return insIt->second;
}

const VulkanResources::VulkanTexture& VulkanResources::getOrCreateFontAtlasTexture( const std::vector<uint8_t>& atlasData, uint32_t width, uint32_t height, const std::string& key)
{
    auto it = fontTextureCache_.find(key);
    if (it != fontTextureCache_.end()) return it->second;

    VulkanTexture tex{};
    vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(atlasData.size());
    auto& staging = createRetainedStagingBuffer(imageSize);
    void* data = staging.memory.mapMemory(0, imageSize);
    memcpy(data, atlasData.data(), static_cast<size_t>(imageSize));
    staging.memory.unmapMemory();

    createImage(width, height, 1, vk::SampleCountFlagBits::e1, vk::Format::eR8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, tex.image, tex.memory);
    transitionImageLayout(tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1);
    copyBufferToImage(staging.buffer, tex.image, width, height);
    transitionImageLayout( tex.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 1);

    tex.view = createImageView(tex.image, vk::Format::eR8Unorm, vk::ImageAspectFlagBits::eColor, 1);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eNearest;
    samplerInfo.minFilter = vk::Filter::eNearest;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    tex.sampler = vk::raii::Sampler(device_->getDevice(), samplerInfo);

    auto [it2, ok] = fontTextureCache_.emplace(key, std::move(tex));
    return it2->second;
}

const VulkanResources::VulkanTexture& VulkanResources::getDefaultCubemapTexture() {
    return getDefaultCubemap();
}

const VulkanResources::VulkanTexture& VulkanResources::getOrCreateCubemapTexture(const std::vector<std::string>& requestedFacePaths) {

    if (isInvalidCubemapPaths(requestedFacePaths)) {
        return getDefaultCubemap();
    }

    std::vector<std::string> facePaths;
    facePaths.reserve(requestedFacePaths.size());
    for (const auto& path : requestedFacePaths) {
        facePaths.push_back(resolveTexturePath(path));
    }

    auto key = makeCubemapCacheKey(facePaths);
    auto it = cubemapCache_.find(key);
    if (it != cubemapCache_.end()) return it->second;

    VulkanTexture tex{};
    
    //Load face 0 to obtain each width and height
    int texWidth = 0, texHeight= 0, channels= 0;
    stbi_uc* pixels = stbi_load(facePaths[0].c_str(), &texWidth, &texHeight, &channels, STBI_rgb_alpha);
    if (!pixels || texWidth <= 0 || texHeight <= 0) {
        stbi_image_free(pixels);
        return getDefaultCubemap();
    }
    stbi_image_free(pixels);

    tex.mipLevels = 1;

    createCubemapImage(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), tex.mipLevels, tex.image, tex.memory);
    transitionCubeMapLayout(tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, tex.mipLevels);

    const int faceWidth = texWidth;
    const int faceHeight = texHeight;
    const vk::DeviceSize faceSize = static_cast<vk::DeviceSize>(faceWidth * faceHeight * 4);

    for (uint32_t face = 0; face < 6; ++face) {
        stbi_uc* pixels = stbi_load(facePaths[face].c_str(), &texWidth, &texHeight, &channels, STBI_rgb_alpha);
        if (!pixels) throw std::runtime_error("failed to load cubemap face: " + facePaths[face]);
        if (texWidth != faceWidth || texHeight != faceHeight) {
            stbi_image_free(pixels);
            throw std::runtime_error("cubemap face size mismatch: " + facePaths[face]);
        }
        auto& staging = createRetainedStagingBuffer(faceSize);

        void* data = staging.memory.mapMemory(0, faceSize);
        memcpy(data, pixels, static_cast<size_t>(faceSize));
        staging.memory.unmapMemory();
        stbi_image_free(pixels);

        copyBufferToImageLayer(staging.buffer, tex.image, static_cast<uint32_t>(faceWidth), static_cast<uint32_t>(faceHeight), face);
    }

    transitionCubeMapLayout(tex.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, tex.mipLevels);

    tex.view = createCubemapImageView(tex.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, tex.mipLevels);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(tex.mipLevels);
    tex.sampler = vk::raii::Sampler(device_->getDevice(), samplerInfo);

    auto [it2, ok] = cubemapCache_.emplace(key, std::move(tex));
    return it2->second;
}

void VulkanResources::generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) 
{
    vk::FormatProperties formatProperties = device_->getPhysicalDevice().getFormatProperties(imageFormat);
    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) 
    {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    auto& commandBuffer = getUploadCommandBuffer();

	vk::ImageMemoryBarrier barrier = {};
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) 
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

        vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
        offsets[0] = vk::Offset3D(0, 0, 0);
        offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
        dstOffsets[0] = vk::Offset3D(0, 0, 0);
        dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);
        vk::ImageBlit blit = {};
        blit.srcSubresource = vk::ImageSubresourceLayers{};
        blit.srcOffsets = offsets;
        blit.dstSubresource =  vk::ImageSubresourceLayers{};
        blit.dstOffsets = dstOffsets;

        blit.srcSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
        blit.dstSubresource = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, i, 0, 1);

        commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
}

void VulkanResources::createTextureImageView() {
    textureImageView_ = createImageView(textureImage_, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels_);
}

void VulkanResources::createTextureSampler() {
    vk::PhysicalDeviceProperties properties = device_->getPhysicalDevice().getProperties();
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = vk::True;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable = vk::False;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = vk::LodClampNone;

    textureSampler_ = vk::raii::Sampler(device_->getDevice(), samplerInfo);
}

void VulkanResources::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                                           vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = vk::Extent3D{width, height, 1};
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = numSamples;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    image = vk::raii::Image(device_->getDevice(), imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    imageMemory = vk::raii::DeviceMemory(device_->getDevice(), allocInfo);
    image.bindMemory(imageMemory, 0);
}

vk::raii::ImageView VulkanResources::createImageView(vk::Image const& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) {
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return vk::raii::ImageView(device_->getDevice(), viewInfo);
}

void VulkanResources::createCubemapImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory) {
    vk::ImageCreateInfo info{};
    info.imageType = vk::ImageType::e2D;
    info.format = vk::Format::eR8G8B8A8Srgb;
    info.extent = vk::Extent3D{ width, height, 1 };
    info.mipLevels = mipLevels;
    info.arrayLayers = 6;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    info.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.initialLayout = vk::ImageLayout::eUndefined;

    image = vk::raii::Image(device_->getDevice(), info);
    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
    imageMemory = vk::raii::DeviceMemory(device_->getDevice(), allocInfo);
    image.bindMemory(imageMemory, 0);

}

vk::raii::ImageView VulkanResources::createCubemapImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels){
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::eCube;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    return vk::raii::ImageView(device_->getDevice(), viewInfo);
}

void VulkanResources::transitionCubeMapLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels){
    auto& commandBuffer = getUploadCommandBuffer();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 6};

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
}


void VulkanResources::transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels) {
    auto& commandBuffer = getUploadCommandBuffer();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout; 
    barrier.image = image;
    barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1};

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
}

void VulkanResources::copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height) {
    auto& commandBuffer = getUploadCommandBuffer();
    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{width, height, 1};


    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
}

void VulkanResources::copyBufferToImageLayer(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height, uint32_t layer){
    auto& commandBuffer = getUploadCommandBuffer();
    vk::BufferImageCopy region{};
    region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, layer, 1 };
    region.imageExtent = vk::Extent3D{ width, height, 1 };
    commandBuffer.copyBufferToImage(buffer, *image, vk::ImageLayout::eTransferDstOptimal, {region});
}

void VulkanResources::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) const {
		vk::BufferCreateInfo bufferInfo{};
		bufferInfo.size                        = size;
		bufferInfo.usage                       = usage;
		bufferInfo.sharingMode                 = vk::SharingMode::eExclusive;
		buffer                                 = vk::raii::Buffer(device_->getDevice(), bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{};
		allocInfo.allocationSize  = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
		bufferMemory              = vk::raii::DeviceMemory(device_->getDevice(), allocInfo);
		buffer.bindMemory(bufferMemory, 0);
}

void VulkanResources::copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size) {
    auto& commandBuffer = getUploadCommandBuffer();
    vk::BufferCopy bufferCopy{};
    bufferCopy.size = size;
    commandBuffer.copyBuffer(*srcBuffer, *dstBuffer, bufferCopy);
}

VulkanMeshGpu VulkanResources::uploadMesh(const std::vector<Vertex>& verts, const std::vector<uint32_t>& indices){
    vk::DeviceSize vertexBufferSize = sizeof(Vertex) * verts.size();
    vk::DeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

    VulkanMeshGpu meshGpuReturnStruct{};
    meshGpuReturnStruct.vertexCount = static_cast<uint32_t>(verts.size());

    if (verts.empty()) {
        return meshGpuReturnStruct;
    }

    auto& vertexStaging = createRetainedStagingBuffer(vertexBufferSize);

    void* dataStaging = vertexStaging.memory.mapMemory(0, vertexBufferSize);
    memcpy(dataStaging, verts.data(), vertexBufferSize);
    vertexStaging.memory.unmapMemory();
    createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, meshGpuReturnStruct.vertexBuffer, meshGpuReturnStruct.vertexBufferMemory);

    copyBuffer(vertexStaging.buffer, meshGpuReturnStruct.vertexBuffer, vertexBufferSize);

    if (!indices.empty()) {
        auto& indexStaging = createRetainedStagingBuffer(indexBufferSize);

        void* data = indexStaging.memory.mapMemory(0, indexBufferSize);
        memcpy(data, indices.data(), static_cast<size_t>(indexBufferSize));
        indexStaging.memory.unmapMemory();

        createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, meshGpuReturnStruct.indexBuffer, meshGpuReturnStruct.indexBufferMemory);

        copyBuffer(indexStaging.buffer, meshGpuReturnStruct.indexBuffer, indexBufferSize);
    }

    meshGpuReturnStruct.indexCount = static_cast<uint32_t>(indices.size());
    return meshGpuReturnStruct;
}

VulkanMeshGpu VulkanResources::uploadMesh(const Mesh& mesh) {
    static_assert(std::is_same_v<std::vector<unsigned int>, std::vector<uint32_t>>,
                  "index upload assumes unsigned int is uint32_t on this target");
    return uploadMesh(mesh.getVertices(), mesh.getIndices());
}

const VulkanMeshGpu& VulkanResources::getOrUploadMesh(const Mesh& mesh) {
    auto found = meshCache_.find(&mesh);
    if (found != meshCache_.end()) {
        return found->second;
    }

    return meshCache_.try_emplace(&mesh, uploadMesh(mesh)).first->second;
}

std::vector<Vertex> VulkanResources::createSkyboxVertices() {
    return {
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}
    };
}

const VulkanMeshGpu& VulkanResources::getSkyboxMesh() {
    if (!skyboxMeshUploaded_) {
        skyboxMeshGpu_ = uploadMesh(createSkyboxVertices(), {});
        skyboxMeshUploaded_ = true;
    }
    return skyboxMeshGpu_;
}

VulkanTextMeshGpu VulkanResources::uploadTextMesh(const std::vector<TextVertex>& verts, const std::vector<uint32_t>& indices){
    vk::DeviceSize vertexBufferSize = sizeof(TextVertex) * verts.size();
    vk::DeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

    VulkanTextMeshGpu meshGpuReturnStruct{};
    meshGpuReturnStruct.vertexCount = static_cast<uint32_t>(verts.size());

    if (verts.empty()) {
        return meshGpuReturnStruct;
    }

    auto& vertexStaging = createRetainedStagingBuffer(vertexBufferSize);

    void* dataStaging = vertexStaging.memory.mapMemory(0, vertexBufferSize);
    memcpy(dataStaging, verts.data(), vertexBufferSize);
    vertexStaging.memory.unmapMemory();
    createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, meshGpuReturnStruct.vertexBuffer, meshGpuReturnStruct.vertexBufferMemory);

    copyBuffer(vertexStaging.buffer, meshGpuReturnStruct.vertexBuffer, vertexBufferSize);

    if (!indices.empty()) {
        auto& indexStaging = createRetainedStagingBuffer(indexBufferSize);

        void* data = indexStaging.memory.mapMemory(0, indexBufferSize);
        memcpy(data, indices.data(), static_cast<size_t>(indexBufferSize));
        indexStaging.memory.unmapMemory();

        createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, meshGpuReturnStruct.indexBuffer, meshGpuReturnStruct.indexBufferMemory);

        copyBuffer(indexStaging.buffer, meshGpuReturnStruct.indexBuffer, indexBufferSize);
    }

    meshGpuReturnStruct.indexCount = static_cast<uint32_t>(indices.size());
    return meshGpuReturnStruct;
}

const VulkanTextMeshGpu& VulkanResources::getOrUploadTextMesh(const TextComponent& text){
    const uint64_t revision = text.getTextMeshRevision();
    auto found = textMeshCache_.find(&text);
    if (found != textMeshCache_.end()) {
        if (found->second.revision == revision) {
            return found->second.gpu;
        }
        retireTextMeshGpu(std::move(found->second.gpu));
        textMeshCache_.erase(found);
    }

    VulkanTextMeshGpu gpu = uploadTextMesh(text.getCpuTextVertices(), text.getCpuIndices());
    textMeshCache_.emplace(&text, CachedTextMesh{ revision, std::move(gpu) });
    return textMeshCache_.at(&text).gpu;
}

const VulkanTextMeshGpu* VulkanResources::findTextMesh(const TextComponent& text) const {
    auto found = textMeshCache_.find(&text);
    if (found == textMeshCache_.end()) {
        return nullptr;
    }
    if (found->second.revision != text.getTextMeshRevision()) {
        return nullptr;
    }
    return &found->second.gpu;
}

void VulkanResources::retireMeshGpu(VulkanMeshGpu&& gpu) {
    if (gpu.vertexCount > 0) {
        retiredMeshes_.push_back(std::move(gpu));
    }
}

void VulkanResources::retireTextMeshGpu(VulkanTextMeshGpu&& gpu) {
    if (gpu.vertexCount > 0) {
        retiredTextMeshes_.push_back(std::move(gpu));
    }
}

void VulkanResources::drainRetiredGpuResources() {
    if (retiredMeshes_.empty() && retiredTextMeshes_.empty()) {
        return;
    }
    waitForGpuIdle();
    retiredMeshes_.clear();
    retiredTextMeshes_.clear();
}

void VulkanResources::clearMeshCache() {
    for (auto& [mesh, gpu] : meshCache_) {
        (void)mesh;
        retireMeshGpu(std::move(gpu));
    }
    meshCache_.clear();

    for (auto& [text, cached] : textMeshCache_) {
        (void)text;
        retireTextMeshGpu(std::move(cached.gpu));
    }
    textMeshCache_.clear();
}

void VulkanResources::evictMesh(const Mesh* mesh) {
    auto it = meshCache_.find(mesh);
    if (it != meshCache_.end()) {
        retireMeshGpu(std::move(it->second));
        meshCache_.erase(it);
    }
}

void VulkanResources::evictTextMesh(const TextComponent* text) {
    auto it = textMeshCache_.find(text);
    if (it != textMeshCache_.end()) {
        retireTextMeshGpu(std::move(it->second.gpu));
        textMeshCache_.erase(it);
    }
}

void VulkanResources::waitForGpuIdle() {
    if (device_) {
        device_->getDevice().waitIdle();
    }
    waitForUploads();
}

void VulkanResources::clearSceneGpuCaches() {
    clearMeshCache();
    drainRetiredGpuResources();
    materialUniformBuffers_.clear();
    shaderMaterialUniformBuffers_.clear();
    textMaterialUniformBuffers_.clear();
}

void VulkanResources::releaseSceneGpuResources() {
    clearSceneGpuCaches();
}


uint32_t VulkanResources::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
    vk::PhysicalDeviceMemoryProperties memProperties = device_->getPhysicalDevice().getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

bool VulkanResources::isInvalidCubemapPaths(const std::vector<std::string>& paths) const{
    return paths.size() != 6 || paths.empty() ||
           std::any_of(
               paths.begin(),
               paths.end(),
               [](const std::string& path) {
                    return path.empty();
                });
}

const VulkanResources::VulkanTexture& VulkanResources::getDefaultCubemap(){
    static const std::string fallbackKey = "__default_cubemap__";
    if (auto it = cubemapCache_.find(fallbackKey); it != cubemapCache_.end()) {
        return it->second;
    }

    VulkanTexture tex{};
    constexpr uint32_t width = 1;
    constexpr uint32_t height = 1;
    tex.mipLevels = 1;

    createCubemapImage(width, height, tex.mipLevels, tex.image, tex.memory);
    transitionCubeMapLayout(tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, tex.mipLevels);

    constexpr std::array<unsigned char, 4> blackPixel = {0, 0, 0, 255};
    const vk::DeviceSize faceSize = static_cast<vk::DeviceSize>(blackPixel.size());

    auto& staging = createRetainedStagingBuffer(faceSize);
    void* data = staging.memory.mapMemory(0, faceSize);
    memcpy(data, blackPixel.data(), static_cast<size_t>(faceSize));
    staging.memory.unmapMemory();

    for (uint32_t face = 0; face < 6; ++face) {
        copyBufferToImageLayer(staging.buffer, tex.image, width, height, face);
    }

    transitionCubeMapLayout(tex.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, tex.mipLevels);
    tex.view = createCubemapImageView(tex.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, tex.mipLevels);


    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(tex.mipLevels);
    tex.sampler = vk::raii::Sampler(device_->getDevice(), samplerInfo);

    auto [it, ok] = cubemapCache_.emplace(fallbackKey, std::move(tex));
    return it->second;
 
}

std::string VulkanResources::makeCubemapCacheKey(const std::vector<std::string>& facePaths) const{
    std::string key = "";
    for (const auto& path : facePaths) {
        key += path + ",";
    }
    return key;
}

vk::raii::CommandPool& VulkanResources::getCommandPool(){
    return commandPool_;
}

std::vector<vk::raii::CommandBuffer>& VulkanResources::getCommandBuffers(){
    return commandBuffers_;
}

vk::raii::Image& VulkanResources::getDepthImage(){
    return depthImage_;
}

vk::raii::ImageView& VulkanResources::getDepthImageView(){
    return depthImageView_;
}

vk::raii::ImageView& VulkanResources::getTextureImageView(){
    return textureImageView_;
}

vk::raii::Sampler& VulkanResources::getTextureSampler(){
    return textureSampler_;
}


}