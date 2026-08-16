#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <unordered_map>
#include "Rendering/Vulkan/VulkanDevice.h"
#include "Rendering/Vulkan/VulkanSwapChain.h"
#include "Rendering/Vulkan/VulkanMeshGpu.h"
#include "Rendering/Mesh.h"
#include "Rendering/RenderTypes.h"

// forward
namespace GameEngine { class Material; }

namespace GameEngine {


class VulkanResources {
public:

    VulkanResources() = default;
    ~VulkanResources() = default;

    void create(VulkanDevice& device, VulkanSwapChain& swapChain);

    void createCommandPool();

    void createDepthResources();

    bool ensureShadowAtlas(uint32_t width, uint32_t height);
    bool hasShadowAtlas() const { return shadowAtlasWidth_ > 0; }
    vk::raii::ImageView& getShadowAtlasImageView() { return shadowAtlasImageView_; }
    vk::Image getShadowAtlasImage() const { return *shadowAtlasImage_; }
    vk::Sampler getShadowAtlasSampler() const { return *shadowAtlasSampler_; }
    vk::Format getShadowAtlasFormat() const { return shadowAtlasFormat_; }
    uint32_t getShadowAtlasWidth() const { return shadowAtlasWidth_; }
    uint32_t getShadowAtlasHeight() const { return shadowAtlasHeight_; }

    // Per-frame uniform buffers (one per swapchain image)
    void createUniformBuffers(vk::DeviceSize size);
    void writeUniformBuffer(uint32_t index, const void* data, vk::DeviceSize size);
    vk::raii::Buffer& getUniformBuffer(uint32_t index);
    vk::raii::DeviceMemory& getUniformBufferMemory(uint32_t index);
    vk::DeviceSize getUniformBufferSize() const { return uniformBufferSize_; }

    void createTextureImage(std::string texturePath);
    void createTextureImageView();
    void createTextureSampler();
    
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                     vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);
    vk::raii::ImageView createImageView(vk::Image const& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);                 

    void createCubemapImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);
    vk::raii::ImageView createCubemapImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);
    void transitionCubeMapLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels);


    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
    vk::Format findDepthFormat();

    void generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    
    void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);
    void copyBufferToImageLayer(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height, uint32_t layer);
    
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) const;
    void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);

    VulkanMeshGpu uploadMesh(const std::vector<Vertex>& verts, const std::vector<uint32_t>& indices);
    VulkanMeshGpu uploadMesh(const Mesh& mesh);

    VulkanTextMeshGpu uploadTextMesh(const std::vector<TextVertex>& verts, const std::vector<uint32_t>& indices);

    const VulkanMeshGpu& getOrUploadMesh(const Mesh& mesh);
    const VulkanMeshGpu& getSkyboxMesh();
    const VulkanTextMeshGpu& getOrUploadTextMesh(const TextComponent& text);
    const VulkanTextMeshGpu* findTextMesh(const TextComponent& text) const;

    void clearMeshCache();

    void evictMesh(const Mesh* mesh);
    void evictTextMesh(const TextComponent* text);

    void waitForGpuIdle();
    void clearSceneGpuCaches();
    // Wait for GPU work to finish, then drop scene owned mesh/text/material GPU caches
    void releaseSceneGpuResources();
    // Destroy retired mesh buffers after the GPU has finished all submitted work
    void drainRetiredGpuResources();

    void waitForUploads();

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

    vk::raii::CommandPool& getCommandPool();
    std::vector<vk::raii::CommandBuffer>& getCommandBuffers();
    vk::raii::Image& getDepthImage();
    vk::raii::ImageView& getDepthImageView();
    vk::raii::ImageView& getTextureImageView();
    vk::raii::Sampler& getTextureSampler();

    struct VulkanTexture {
        vk::raii::Image image{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::raii::ImageView view{nullptr};
        vk::raii::Sampler sampler{nullptr};
        uint32_t mipLevels = 1;
    };

    const VulkanTexture& getOrCreateTexture(const std::string& path);
    const VulkanTexture& getOrCreateTextureFromMemory(const uint8_t* data, uint32_t width, uint32_t height, int channels, const std::string& cacheKey);
    static bool isEmbeddedTextureKey(const std::string& key);
    const VulkanTexture& getOrCreateFontAtlasTexture( const std::vector<uint8_t>& atlasData, uint32_t width, uint32_t height, const std::string& key);
    const VulkanTexture& getOrCreateCubemapTexture(const std::vector<std::string>& facePaths);
    const VulkanTexture& getDefaultCubemapTexture();

    struct MaterialBuffer {
        vk::raii::Buffer buffer{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::DeviceSize size = 0;
    };

    struct TextMaterialBuffer {
        vk::raii::Buffer buffer{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::DeviceSize size = 0;
    };

    struct ShaderMaterialBuffer {
        vk::raii::Buffer buffer{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::DeviceSize size = 0;
    };

    struct AnimationBuffer {
        vk::raii::Buffer buffer{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::DeviceSize size = 0;
    };

    void ensureMaterialUniformBuffer(const Material* material, uint32_t imageIndex, const MaterialUniforms& data);
    void ensureShaderMaterialUniformBuffer(const Material* material, uint32_t imageIndex, const ShaderMaterialUniforms& data);
    void ensureTextMaterialUniformBuffer(const TextMaterial* material, const TextMaterialUniforms& data);
    void createAnimationUniformBuffers();
    void writeAnimationUniform(uint32_t slot, const std::vector<glm::mat4>& boneTransforms);

    vk::DescriptorBufferInfo getMaterialDescriptorBufferInfo(const Material* material, uint32_t imageIndex) const;

    uint32_t getFrameSlotCount() const;
    vk::DescriptorBufferInfo getShaderMaterialDescriptorBufferInfo(const Material* material, uint32_t imageIndex) const;
    vk::DescriptorBufferInfo getTextMaterialDescriptorBufferInfo(const TextMaterial* material) const;
    vk::DescriptorBufferInfo getAnimationDescriptorBufferInfo(uint32_t slot) const;

    std::vector<vk::raii::Buffer> uniformBuffers_;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory_;
    std::vector<void*> uniformBuffersMapped_;
    vk::DeviceSize uniformBufferSize_ = 0;

private:
    VulkanDevice* device_ = nullptr;
    VulkanSwapChain* swapChain_ = nullptr;

    vk::raii::CommandPool commandPool_ = nullptr;
    vk::raii::CommandBuffer uploadCommandBuffer_{nullptr};
    vk::raii::Fence uploadFence_{nullptr};
    bool uploadBatchActive_ = false;
    bool uploadInFlight_ = false;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::CommandBuffer> computeCommandBuffers_;

    void ensureUploadResources();
    vk::raii::CommandBuffer& getUploadCommandBuffer();
    void flushUploads();

    struct RetainedStagingBuffer {
        vk::raii::Buffer buffer{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
    };
    std::vector<RetainedStagingBuffer> uploadStagingRetention_;
    RetainedStagingBuffer& createRetainedStagingBuffer(vk::DeviceSize size);

    vk::raii::Image depthImage_ = nullptr;
    vk::raii::DeviceMemory depthImageMemory_ = nullptr;
    vk::raii::ImageView depthImageView_ = nullptr;

    vk::raii::Image shadowAtlasImage_ = nullptr;
    vk::raii::DeviceMemory shadowAtlasMemory_ = nullptr;
    vk::raii::ImageView shadowAtlasImageView_ = nullptr;
    vk::raii::Sampler shadowAtlasSampler_ = nullptr;
    vk::Format shadowAtlasFormat_ = vk::Format::eUndefined;
    uint32_t shadowAtlasWidth_ = 0;
    uint32_t shadowAtlasHeight_ = 0;

    uint32_t mipLevels_ = 0;
    vk::raii::Image textureImage_ = nullptr;
    vk::raii::DeviceMemory textureImageMemory_ = nullptr;
    vk::raii::ImageView textureImageView_ = nullptr;
    vk::raii::Sampler textureSampler_ = nullptr;

    std::unordered_map<const Mesh*, VulkanMeshGpu> meshCache_;
    struct CachedTextMesh {
        uint64_t revision = 0;
        VulkanTextMeshGpu gpu;
    };
    std::unordered_map<const TextComponent*, CachedTextMesh> textMeshCache_;
    std::vector<VulkanMeshGpu> retiredMeshes_;
    std::vector<VulkanTextMeshGpu> retiredTextMeshes_;
    void retireMeshGpu(VulkanMeshGpu&& gpu);
    void retireTextMeshGpu(VulkanTextMeshGpu&& gpu);

    std::unordered_map<std::string, VulkanTexture> textureCache_;
    std::unordered_map<std::string, VulkanTexture> fontTextureCache_;
    std::unordered_map<std::string, VulkanTexture> cubemapCache_;
    
    std::unordered_map<const Material*, std::vector<MaterialBuffer>> materialUniformBuffers_;
    std::unordered_map<const Material*, std::vector<ShaderMaterialBuffer>> shaderMaterialUniformBuffers_;
    std::unordered_map<const TextMaterial*, TextMaterialBuffer> textMaterialUniformBuffers_;
    std::vector<AnimationBuffer> animationUniformBuffers_;

    VulkanMeshGpu skyboxMeshGpu_{};
    bool skyboxMeshUploaded_ = false;

    static std::vector<Vertex> createSkyboxVertices();
    static std::string resolveTexturePath(const std::string& path);

    bool isInvalidCubemapPaths(const std::vector<std::string>& paths) const;
    const VulkanTexture& getDefaultCubemap();
    std::string makeCubemapCacheKey(const std::vector<std::string>& facePaths) const;
};
}