
#pragma once
#include <vulkan/vulkan_raii.hpp>
#include "Rendering/Mesh.h"
#include "Rendering/Vulkan/VulkanDevice.h"
#include "Rendering/Vulkan/VulkanResources.h"
#include "Rendering/Vulkan/VulkanSwapChain.h"
#include "Rendering/RenderTypes.h"
#include "Rendering/Material.h"
#include <span>
#include <unordered_map>
#include <string>

constexpr bool ENABLE_PARTICLE_COMPUTE = false;

namespace GameEngine {

class VulkanSwapChain;
class Material;

class VulkanPipeline {

public:
    struct CachedShaderPipeline {
        vk::raii::Pipeline pipeline{nullptr};
        vk::raii::PipelineLayout layout{nullptr};
    };

    void create(VulkanDevice& device, VulkanResources& vulkanResources, VulkanSwapChain& swapchain);

    std::vector<char> readFile(const std::string& filename);

    vk::raii::PipelineLayout& getGraphicsPipelineLayout();
    vk::raii::Pipeline& getGraphicsPipeline();
    vk::raii::Pipeline& getGraphicsPipeline(BlendMode blendMode, bool depthWrite, bool cullEnabled = true);
    vk::raii::PipelineLayout& getTextPipelineLayout();
    vk::raii::Pipeline& getTextPipeline();
    vk::raii::PipelineLayout& getSkyboxPipelineLayout();
    vk::raii::Pipeline& getSkyboxPipeline();
    vk::raii::PipelineLayout& getBeamPipelineLayout();
    vk::raii::Pipeline& getBeamPipeline();
    vk::raii::Pipeline& getShadowPipeline();

    void refreshShadowAtlasBinding();

    const CachedShaderPipeline* getCustomShaderPipeline(const Material* material);
    vk::DescriptorSet getOrCreateShaderMaterialDescriptorSet(const Material* material);

    vk::DescriptorSet getDescriptorSet(uint32_t index);
    void recreateDescriptorSets();
    vk::DescriptorSet getOrCreateMaterialDescriptorSet(const Material* materialKey);
    vk::DescriptorSet getOrUpdateEnvironmentDescriptorSet( const FrameEnvironment& env, const VulkanResources::VulkanTexture& cubemap);
    vk::DescriptorSet getOrCreateTextDescriptorSet(const TextMaterial* material, const VulkanResources::VulkanTexture& atlasTexture);
    vk::DescriptorSet getAnimationDescriptorSet(uint32_t slot) const;
    void clearSceneDescriptorCaches();

private:

    void createDescriptorSetLayout();
    void createShaderMaterialDescriptorSetLayout();
    void createAnimationDescriptorSetLayout();
    void createTextDescriptorSetLayout();
    void createGraphicsPipeline();
    void createTextPipeline();
    void createSkyboxPipeline();
    void createBeamPipeline();
    void createShadowPipeline();

    void createParticleGraphicsPipeline();

    CachedShaderPipeline createShaderGraphicsPipeline(
        const std::string& vertSpvPath,
        const std::string& fragSpvPath,
        BlendMode blendMode,
        bool depthWrite,
        bool cullEnabled,
        uint32_t pushConstantSize,
        vk::ShaderStageFlags pushConstantStages,
        std::span<const vk::VertexInputAttributeDescription> vertexAttributes);

    CachedShaderPipeline& getOrCreateCustomShaderPipeline(const Material* material);
    static bool shaderSpvExists(const std::string& path);
    static std::string makeCustomPipelineCacheKey(const Material* material);
    static vk::PipelineColorBlendAttachmentState makeBlendAttachment(BlendMode blendMode);
    static uint32_t makeDefaultLitPipelineKey(BlendMode blendMode, bool depthWrite, bool cullEnabled);

    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

    static vk::VertexInputBindingDescription getMeshVertexBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 6> getMeshVertexAttributeDescriptions();
    static std::array<vk::VertexInputAttributeDescription, 1> getSkyboxVertexAttributeDescriptions();
    static std::array<vk::VertexInputAttributeDescription, 2> getShaderMaterialVertexAttributeDescriptions();

    static vk::VertexInputBindingDescription getTextVertexBindingDescription();
    static std::array<vk::VertexInputAttributeDescription,2> getTextVertexAttributeDescriptions();

    //Not owner
    VulkanDevice* device_ = nullptr;
    VulkanResources* resources_ = nullptr;
    VulkanSwapChain* swapChain_ = nullptr;

    //Owner
    vk::raii::DescriptorSetLayout frameDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout materialDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout shaderMaterialDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout environmentDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout animationDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout textDescriptorSetLayout_ = nullptr;

    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline graphicsPipeline_ = nullptr;
    std::unordered_map<uint32_t, vk::raii::Pipeline> defaultLitPipelineCache_;
    vk::raii::PipelineLayout textPipelineLayout_ = nullptr;
    vk::raii::Pipeline textPipeline_ = nullptr;
    vk::raii::PipelineLayout skyboxPipelineLayout_ = nullptr;
    vk::raii::Pipeline skyboxPipeline_ = nullptr;
    vk::raii::PipelineLayout beamPipelineLayout_ = nullptr;
    vk::raii::Pipeline beamPipeline_ = nullptr;
    vk::raii::Pipeline shadowPipeline_ = nullptr;

    vk::raii::PipelineLayout particlePipelineLayout_ = nullptr;
    vk::raii::Pipeline particlePipeline_ = nullptr;
    // Descriptor pool and per-frame descriptor sets for per-frame UBOs
    vk::raii::DescriptorPool descriptorPool_ = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets_;
    std::unordered_map<const Material*, std::unique_ptr<vk::raii::DescriptorSet>> materialDescriptorSets_;
    std::unordered_map<const Material*, std::unique_ptr<vk::raii::DescriptorSet>> shaderMaterialDescriptorSets_;
    std::unordered_map<const TextMaterial*, std::unique_ptr<vk::raii::DescriptorSet>> textMaterialDescriptorSets_;
    std::unique_ptr<vk::raii::DescriptorSet> environmentDescriptorSet_;
    const void* currentEnvironmentKey_ = nullptr;
    std::vector<vk::raii::DescriptorSet> animationDescriptorSets_;
    std::unordered_map<std::string, CachedShaderPipeline> customShaderPipelineCache_;

    void createDescriptorPoolAndSets();
    void createAnimationDescriptorSets();
};
}
