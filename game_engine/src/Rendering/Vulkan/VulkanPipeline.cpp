#include "Rendering/Vulkan/VulkanPipeline.h"
#include "Rendering/Material.h"
#include "Rendering/TextMaterial.h"
#include <fstream>
#include <glm/glm.hpp>

namespace GameEngine {

namespace {
constexpr const char* kDefaultVertexShaderSpvPath = "assets/vulkan/default_lit.vert.spv";
constexpr const char* kDefaultFragmentShaderSpvPath = "assets/vulkan/default_lit.frag.spv";
constexpr const char* kTextVertexShaderSpvPath = "assets/vulkan/text.vert.spv";
constexpr const char* kTextFragmentShaderSpvPath = "assets/vulkan/text.frag.spv";
constexpr const char* kSkyboxVertexShaderSpvPath = "assets/vulkan/skybox.vert.spv";
constexpr const char* kSkyboxFragmentShaderSpvPath = "assets/vulkan/skybox.frag.spv";
}

void VulkanPipeline::create(VulkanDevice& device, VulkanResources& vulkanResources, VulkanSwapChain& swapchain){
    device_ = &device;
    resources_ = &vulkanResources;
    swapChain_ = &swapchain;

    createDescriptorSetLayout();
    createTextDescriptorSetLayout();

    createGraphicsPipeline();
    createTextPipeline();
    createSkyboxPipeline();

    createDescriptorPoolAndSets();
    if (ENABLE_PARTICLE_COMPUTE){
        createParticleGraphicsPipeline();
    }
}

void VulkanPipeline::createDescriptorSetLayout() {
    // Frame descriptor layout: binding 0 = UBO
    std::array frameBindings = { vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, nullptr) };
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
    uint32_t materialDescriptorCapacity = std::max<uint32_t>(imageCount * 64, imageCount);
    // Reserve uniform buffers for frame UBOs + material UBOs + one environment set
    std::array poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, imageCount + materialDescriptorCapacity),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, materialDescriptorCapacity * 3 + 1)
    };

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = imageCount + materialDescriptorCapacity + 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    materialDescriptorSets_.clear();
    textMaterialDescriptorSets_.clear();
    descriptorSets_.clear();
    environmentDescriptorSet_.reset();
    currentEnvironmentKey_.clear();
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
}

vk::DescriptorSet VulkanPipeline::getOrCreateMaterialDescriptorSet(const Material* material) {
    auto it = materialDescriptorSets_.find(material);
    if (it != materialDescriptorSets_.end()) return *it->second;

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *descriptorPool_;
    vk::DescriptorSetLayout layout = *materialDescriptorSetLayout_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    auto sets = device_->getDevice().allocateDescriptorSets(allocInfo);
    vk::raii::DescriptorSet set = std::move(sets.front());

    if (material) {
        MaterialUniforms mu{};
        mu.baseColor = glm::vec4(material->getColorLinear(), 1.0f);
        mu.roughness = material->getRoughness();
        mu.metallic = material->getMetallic();
        mu.reflectionStrength = material->getReflectionStrength();
        mu.padding = 0.0f;
        mu.textureFlags = glm::vec4(
            material->hasDiffuseTexture() ? 1.0f : 0.0f,
            material->hasNormalTexture() ? 1.0f : 0.0f,
            material->hasARMTexture() ? 1.0f : 0.0f,
            0.0f);
        resources_->ensureMaterialUniformBuffer(material, mu);
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

    vk::DescriptorBufferInfo bufferInfo = resources_->getMaterialDescriptorBufferInfo(material);

    std::array<vk::WriteDescriptorSet, 4> writes{};

    writes[0].dstSet = *set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &diffuseInfo;

    writes[1].dstSet = *set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &normalInfo;

    writes[2].dstSet = *set;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &armInfo;

    writes[3].dstSet = *set;
    writes[3].dstBinding = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = vk::DescriptorType::eUniformBuffer;
    writes[3].descriptorCount = 1;
    writes[3].pBufferInfo = &bufferInfo;

    device_->getDevice().updateDescriptorSets(writes, {});

    materialDescriptorSets_.emplace(material, std::make_unique<vk::raii::DescriptorSet>(std::move(set)));
    return *materialDescriptorSets_[material].get();
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
        currentEnvironmentKey_.clear();
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

vk::DescriptorSet VulkanPipeline::getOrCreateTextDescriptorSet(const TextMaterial* material, const VulkanResources::VulkanTexture& atlasTexture){
    auto it = textMaterialDescriptorSets_.find(material);
    if (it != textMaterialDescriptorSets_.end()) return *it->second;

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *descriptorPool_;
    vk::DescriptorSetLayout layout = *textDescriptorSetLayout_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    auto sets = device_->getDevice().allocateDescriptorSets(allocInfo);
    vk::raii::DescriptorSet set = std::move(sets.front());

    if (material) {
        TextMaterialUniforms mu{};
        mu.color = material->getColor();
        resources_->ensureTextMaterialUniformBuffer(material, mu);
    }
    vk::DescriptorImageInfo diffuseInfo{};
    diffuseInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    diffuseInfo.imageView = *atlasTexture.view;
    diffuseInfo.sampler = *atlasTexture.sampler;


    vk::DescriptorBufferInfo bufferInfo = resources_->getTextMaterialDescriptorBufferInfo(material);
    std::array<vk::WriteDescriptorSet, 2> writes{};

    writes[0].dstSet = *set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &diffuseInfo;

    writes[1].dstSet = *set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = vk::DescriptorType::eUniformBuffer;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &bufferInfo;

    device_->getDevice().updateDescriptorSets({writes[0], writes[1]}, {});
    textMaterialDescriptorSets_.emplace(material, std::make_unique<vk::raii::DescriptorSet>(std::move(set)));
    return *textMaterialDescriptorSets_[material].get();

}

void VulkanPipeline::recreateDescriptorSets() {
    createDescriptorPoolAndSets();
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
    // Current dynamic-rendering path targets 1-sample swapchain/depth attachments directly.
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = vk::True;
    depthStencil.depthWriteEnable = vk::True;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
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

    vk::PushConstantRange pushConstant{};
    pushConstant.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pushConstant.offset = 0;
    //pushConstant.size = sizeof(glm::mat4);
    pushConstant.size = sizeof(PushConstants);

    std::array<const vk::DescriptorSetLayout, 3> setLayouts = {
        *frameDescriptorSetLayout_,
        *materialDescriptorSetLayout_,
        *environmentDescriptorSetLayout_
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    pipelineLayout_ = vk::raii::PipelineLayout(device_->getDevice(), pipelineLayoutInfo);

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
    pipelineCreateInfo.layout = pipelineLayout_;
    pipelineCreateInfo.renderPass = nullptr;

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
        pipelineCreateInfo,
        pipelineRenderingInfo
    };

    graphicsPipeline_ = vk::raii::Pipeline(device_->getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void VulkanPipeline::createTextPipeline(){
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
    depthStencil.depthTestEnable = vk::False;
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

    textPipeline_ = vk::raii::Pipeline(device_->getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
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

vk::raii::PipelineLayout& VulkanPipeline::getTextPipelineLayout(){
    return textPipelineLayout_;
}

vk::raii::Pipeline& VulkanPipeline::getTextPipeline(){
    return textPipeline_;
}

vk::raii::PipelineLayout& VulkanPipeline::getSkyboxPipelineLayout(){
    return skyboxPipelineLayout_;
}

vk::raii::Pipeline& VulkanPipeline::getSkyboxPipeline(){
    return skyboxPipeline_;
}

vk::DescriptorSet VulkanPipeline::getDescriptorSet(uint32_t index) {
    if (index >= descriptorSets_.size()) throw std::out_of_range("descriptor set index out of range");
    return *descriptorSets_[index];
}

std::vector<char> VulkanPipeline::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

}