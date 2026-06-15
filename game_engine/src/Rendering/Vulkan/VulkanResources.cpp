#include "Rendering/Vulkan/VulkanResources.h"
#include "Rendering/Material.h"
#include "../../vendor/tinygltf/stb_image.h"
#include <stdexcept>
#include <iostream>

namespace GameEngine {

void VulkanResources::create(VulkanDevice& device, VulkanSwapChain& swapChain){
    device_ = &device;
    swapChain_ = &swapChain;
    createCommandPool();
    createColorResources();
    createDepthResources();
    // Create per-frame uniform buffers sized for camera/projection
    createUniformBuffers(sizeof(PerFrameUniforms));
}

void VulkanResources::ensureMaterialUniformBuffer(const Material* material, const MaterialUniforms& data) {
    if (!material) return;
    auto it = materialUniformBuffers_.find(material);
    vk::DeviceSize dataSize = sizeof(MaterialUniforms);
    if (it != materialUniformBuffers_.end()) {
        // Update existing buffer
        auto& mb = it->second;
        if (mb.size == dataSize) {
            void* mem = mb.memory.mapMemory(0, dataSize);
            memcpy(mem, &data, static_cast<size_t>(dataSize));
            mb.memory.unmapMemory();
            return;
        }
    }

    vk::raii::Buffer buf = nullptr;
    vk::raii::DeviceMemory mem = nullptr;
    createBuffer(dataSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buf, mem);
    void* mapped = mem.mapMemory(0, dataSize);
    memcpy(mapped, &data, static_cast<size_t>(dataSize));
    mem.unmapMemory();

    MaterialBuffer mb{};
    mb.buffer = std::move(buf);
    mb.memory = std::move(mem);
    mb.size = dataSize;

    materialUniformBuffers_.emplace(material, std::move(mb));
}

void VulkanResources::ensureTextMaterialUniformBuffer(const TextMaterial* material, const TextMaterialUniforms& data) {
    if (!material) return;
    auto it = textMaterialUniformBuffers_.find(material);
    vk::DeviceSize dataSize = sizeof(TextMaterialUniforms);
    if (it != textMaterialUniformBuffers_.end()) {
        // Update existing buffer
        auto& mb = it->second;
        if (mb.size == dataSize) {
            void* mem = mb.memory.mapMemory(0, dataSize);
            memcpy(mem, &data, static_cast<size_t>(dataSize));
            mb.memory.unmapMemory();
            return;
        }
    }

    vk::raii::Buffer buf = nullptr;
    vk::raii::DeviceMemory mem = nullptr;
    createBuffer(dataSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buf, mem);
    void* mapped = mem.mapMemory(0, dataSize);
    memcpy(mapped, &data, static_cast<size_t>(dataSize));
    mem.unmapMemory();

    TextMaterialBuffer mb{};
    mb.buffer = std::move(buf);
    mb.memory = std::move(mem);
    mb.size = dataSize;

    textMaterialUniformBuffers_.emplace(material, std::move(mb));
}

vk::DescriptorBufferInfo VulkanResources::getMaterialDescriptorBufferInfo(const Material* material) const {
    vk::DescriptorBufferInfo info{};
    if (!material) return info;
    auto it = materialUniformBuffers_.find(material);
    if (it == materialUniformBuffers_.end()) return info;
    info.buffer = *it->second.buffer;
    info.offset = 0;
    info.range = it->second.size;
    return info;
}

vk::DescriptorBufferInfo VulkanResources::getTextMaterialDescriptorBufferInfo(const TextMaterial* material) const {
    vk::DescriptorBufferInfo info{};
    if (!material) return info;
    auto it = textMaterialUniformBuffers_.find(material);
    if (it == textMaterialUniformBuffers_.end()) return info;
    info.buffer = *it->second.buffer;
    info.offset = 0;
    info.range = it->second.size;
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

void VulkanResources::createColorResources() {
    vk::Format colorFormat = swapChain_->getSurfaceFormat().format;

    createImage(swapChain_->getExtent().width, swapChain_->getExtent().height, 1, device_->getMsaaSamples(), colorFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,  vk::MemoryPropertyFlagBits::eDeviceLocal, colorImage_, colorImageMemory_);
    colorImageView_ = createImageView(colorImage_, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
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
    stbi_uc* pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
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

    auto it = textureCache_.find(path);
    if (it != textureCache_.end()) return it->second;

    VulkanTexture tex{};

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) throw std::runtime_error("failed to load texture: " + path);
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

    auto [insIt, ok] = textureCache_.try_emplace(path, std::move(tex));
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

std::string VulkanResources::getCubemapCacheKey(const std::vector<std::string>& facePaths) const {
    return makeCubemapCacheKey(facePaths);
}

const VulkanResources::VulkanTexture& VulkanResources::getOrCreateCubemapTexture(const std::vector<std::string>& facePaths) {

    if (isInvalidCubemapPaths(facePaths)) {
        return getDefaultCubemap();
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

    const vk::DeviceSize faceSize = static_cast<vk::DeviceSize>(texWidth * texHeight * 4);

    for (uint32_t face = 0; face < 6; ++face) {
        stbi_uc* pixels = stbi_load(facePaths[face].c_str(), &texWidth, &texHeight, &channels, STBI_rgb_alpha);
        if (!pixels) throw std::runtime_error("failed to load cubemap face: " + facePaths[face]);
        auto& staging = createRetainedStagingBuffer(faceSize);

        void* data = staging.memory.mapMemory(0, faceSize);
        memcpy(data, pixels, static_cast<size_t>(faceSize));
        staging.memory.unmapMemory();
        stbi_image_free(pixels);

        copyBufferToImageLayer(staging.buffer, tex.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), face);
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
    std::vector<uint32_t> indices;
    indices.reserve(mesh.getIndices().size());
    for (unsigned int index : mesh.getIndices()) {
        indices.push_back(static_cast<uint32_t>(index));
    }

    return uploadMesh(mesh.getVertices(), indices);
}

const VulkanMeshGpu& VulkanResources::getOrUploadMesh(const Mesh& mesh) {
    auto found = meshCache_.find(&mesh);
    if (found != meshCache_.end()) {
        return found->second;
    }

    return meshCache_.try_emplace(&mesh, uploadMesh(mesh)).first->second;
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
    auto found = textMeshCache_.find(&text);
    if (found != textMeshCache_.end()) {
        return found->second;
    }

    std::vector<TextVertex> tvs = text.getCpuTextVertices();
    std::vector<uint32_t> idxs;
    idxs.reserve(text.getCpuIndices().size());
    for (unsigned int i : text.getCpuIndices()) idxs.push_back(static_cast<uint32_t>(i));

    VulkanTextMeshGpu gpu = uploadTextMesh(tvs, idxs);
    auto insRes = textMeshCache_.try_emplace(&text, std::move(gpu));
    return insRes.first->second;
}

void VulkanResources::clearMeshCache() {
    meshCache_.clear();
    textMeshCache_.clear();
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

vk::raii::Image& VulkanResources::getColorImage(){
    return colorImage_;
}

vk::raii::ImageView& VulkanResources::getColorImageView(){
    return colorImageView_;
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