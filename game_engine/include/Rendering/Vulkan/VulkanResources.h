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

    void createColorResources();
    void createDepthResources();

    // Per-frame uniform buffers (one per swapchain image)
    void createUniformBuffers(vk::DeviceSize size);
    vk::raii::Buffer& getUniformBuffer(uint32_t index);
    vk::raii::DeviceMemory& getUniformBufferMemory(uint32_t index);
    vk::DeviceSize getUniformBufferSize() const { return uniformBufferSize_; }

    void createTextureImage(std::string texturePath);
    void createTextureImageView();
    void createTextureSampler();
    vk::raii::ImageView createImageView(vk::Image const& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                     vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);

    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
    vk::Format findDepthFormat();

    void generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    
    void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);
    
    
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) const;
    void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);

    VulkanMeshGpu uploadMesh(const std::vector<Vertex>& verts, const std::vector<uint32_t>& indices);
    VulkanMeshGpu uploadMesh(const Mesh& mesh);

    VulkanTextMeshGpu uploadTextMesh(const std::vector<TextVertex>& verts, const std::vector<uint32_t>& indices);

    const VulkanMeshGpu& getOrUploadMesh(const Mesh& mesh);
    const VulkanTextMeshGpu& getOrUploadTextMesh(const TextComponent& text);

    void clearMeshCache();

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

    std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();
    void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer);

    
    vk::raii::CommandPool& getCommandPool();
    std::vector<vk::raii::CommandBuffer>& getCommandBuffers();
    vk::raii::Image& getColorImage();
    vk::raii::ImageView& getColorImageView();
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
    const VulkanTexture& getOrCreateFontAtlasTexture( const std::vector<uint8_t>& atlasData, uint32_t width, uint32_t height, const std::string& key);

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

    void ensureMaterialUniformBuffer(const Material* material, const MaterialUniforms& data);
    void ensureTextMaterialUniformBuffer(const TextMaterial* material, const TextMaterialUniforms& data);

    
    vk::DescriptorBufferInfo getMaterialDescriptorBufferInfo(const Material* material) const;
    vk::DescriptorBufferInfo getTextMaterialDescriptorBufferInfo(const TextMaterial* material) const;

    std::vector<vk::raii::Buffer> uniformBuffers_;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory_;
    vk::DeviceSize uniformBufferSize_ = 0;

private:
    VulkanDevice* device_ = nullptr;
    VulkanSwapChain* swapChain_ = nullptr;

    vk::raii::CommandPool commandPool_ = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::CommandBuffer> computeCommandBuffers_;

    vk::raii::Image colorImage_ = nullptr;
    vk::raii::DeviceMemory colorImageMemory_ = nullptr;
    vk::raii::ImageView colorImageView_ = nullptr;

    vk::raii::Image depthImage_ = nullptr;
    vk::raii::DeviceMemory depthImageMemory_ = nullptr;
    vk::raii::ImageView depthImageView_ = nullptr;

    uint32_t mipLevels_ = 0;
    vk::raii::Image textureImage_ = nullptr;
    vk::raii::DeviceMemory textureImageMemory_ = nullptr;
    vk::raii::ImageView textureImageView_ = nullptr;
    vk::raii::Sampler textureSampler_ = nullptr;

    std::unordered_map<const Mesh*, VulkanMeshGpu> meshCache_;
    std::unordered_map<const TextComponent*, VulkanTextMeshGpu> textMeshCache_;
    std::unordered_map<std::string, VulkanTexture> textureCache_;
    std::unordered_map<std::string, VulkanTexture> fontTextureCache_;
    std::unordered_map<const Material*, MaterialBuffer> materialUniformBuffers_;
    std::unordered_map<const TextMaterial*, TextMaterialBuffer> textMaterialUniformBuffers_;

};
}