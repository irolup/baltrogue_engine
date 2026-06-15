
#pragma once
#include <vulkan/vulkan_raii.hpp>
#include "Rendering/Mesh.h"
#include "Rendering/Vulkan/VulkanDevice.h"
#include "Rendering/Vulkan/VulkanResources.h"
#include "Rendering/Vulkan/VulkanSwapChain.h"
#include "Rendering/RenderTypes.h"

constexpr bool ENABLE_PARTICLE_COMPUTE = false;

namespace GameEngine {

class VulkanSwapChain;
class Material;

class VulkanPipeline {

public:
    void create(VulkanDevice& device, VulkanResources& vulkanResources, VulkanSwapChain& swapchain);


    std::vector<char> readFile(const std::string& filename);

    vk::raii::PipelineLayout& getGraphicsPipelineLayout();
    vk::raii::Pipeline& getGraphicsPipeline();
    vk::raii::PipelineLayout& getTextPipelineLayout();
    vk::raii::Pipeline& getTextPipeline();

    vk::DescriptorSet getDescriptorSet(uint32_t index);
    void recreateDescriptorSets();
    vk::DescriptorSet getOrCreateMaterialDescriptorSet(const Material* materialKey);
    vk::DescriptorSet getOrUpdateEnvironmentDescriptorSet( const FrameEnvironment& env, const VulkanResources::VulkanTexture& cubemap);
    vk::DescriptorSet getOrCreateTextDescriptorSet(const TextMaterial* material, const VulkanResources::VulkanTexture& atlasTexture);

private:

    void createDescriptorSetLayout();
    void createTextDescriptorSetLayout();
    void createGraphicsPipeline();
    void createTextPipeline();


    void createParticleGraphicsPipeline();

    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

    static vk::VertexInputBindingDescription getMeshVertexBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 6> getMeshVertexAttributeDescriptions();

    static vk::VertexInputBindingDescription getTextVertexBindingDescription();
    static std::array<vk::VertexInputAttributeDescription,2> getTextVertexAttributeDescriptions();

    //Not owner
    VulkanDevice* device_ = nullptr;
    VulkanResources* resources_ = nullptr;
    VulkanSwapChain* swapChain_ = nullptr;

    //Owner
    vk::raii::DescriptorSetLayout frameDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout materialDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout environmentDescriptorSetLayout_ = nullptr;
    vk::raii::DescriptorSetLayout textDescriptorSetLayout_ = nullptr;

    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline graphicsPipeline_ = nullptr;
    vk::raii::PipelineLayout textPipelineLayout_ = nullptr;
    vk::raii::Pipeline textPipeline_ = nullptr;

    vk::raii::PipelineLayout particlePipelineLayout_ = nullptr;
    vk::raii::Pipeline particlePipeline_ = nullptr;
    // Descriptor pool and per-frame descriptor sets for per-frame UBOs
    vk::raii::DescriptorPool descriptorPool_ = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets_;
    std::unordered_map<const Material*, std::unique_ptr<vk::raii::DescriptorSet>> materialDescriptorSets_;
    std::unordered_map<const TextMaterial*, std::unique_ptr<vk::raii::DescriptorSet>> textMaterialDescriptorSets_;
    std::unique_ptr<vk::raii::DescriptorSet> environmentDescriptorSet_;
    std::string currentEnvironmentKey_;

    void createDescriptorPoolAndSets();
};
}