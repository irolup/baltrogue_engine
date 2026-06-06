#include "Rendering/Vulkan/VulkanCompute.h"

namespace GameEngine {

void VulkanCompute::initCompute() {
    createShaderStorageBuffers();
    createComputeUniformBuffers();
    createComputeDescriptorSetLayout();
    createComputeDescriptorPool();
    createComputeDescriptorSets();
    createComputePipeline();
    createComputeCommandBuffers();
}

void VulkanCompute::createShaderStorageBuffers() {
    // std::default_random_engine rndEngine(static_cast<unsigned>(std::time(nullptr)));
    // std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    // // Demo initial layout — move spawn logic to engine ParticleSystem later.
    // std::vector<Particle> particles(PARTICLE_COUNT);
    // for (auto& particle : particles) {
    //     float r = 0.25f * std::sqrt(rndDist(rndEngine));
    //     float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
    //     float x = r * std::cos(theta) * static_cast<float>(HEIGHT) / static_cast<float>(WIDTH);
    //     float y = r * std::sin(theta);
    //     particle.position = glm::vec2(x, y);
    //     particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
    //     particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    // }

    // vk::DeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

    // vk::raii::Buffer stagingBuffer = nullptr;
    // vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    // createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
    //              vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer,
    //              stagingBufferMemory);

    // void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
    // std::memcpy(dataStaging, particles.data(), static_cast<size_t>(bufferSize));
    // stagingBufferMemory.unmapMemory();

    // shaderStorageBuffers.clear();
    // shaderStorageBuffersMemory.clear();

    // for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    //     vk::raii::Buffer shaderStorageBufferTemp = nullptr;
    //     vk::raii::DeviceMemory shaderStorageBufferTempMemory = nullptr;
    //     createBuffer(bufferSize,
    //                  vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer |
    //                      vk::BufferUsageFlagBits::eTransferDst,
    //                  vk::MemoryPropertyFlagBits::eDeviceLocal, shaderStorageBufferTemp, shaderStorageBufferTempMemory);
    //     copyBuffer(stagingBuffer, shaderStorageBufferTemp, bufferSize);
    //     shaderStorageBuffers.emplace_back(std::move(shaderStorageBufferTemp));
    //     shaderStorageBuffersMemory.emplace_back(std::move(shaderStorageBufferTempMemory));
    // }
}

void VulkanCompute::createComputeUniformBuffers() {
    // computeUniformBuffers.clear();
    // computeUniformBuffersMemory.clear();
    // computeUniformBuffersMapped.clear();

    // for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    //     vk::DeviceSize bufferSize = sizeof(UniformBufferObjectCompute);

    //     vk::raii::Buffer buffer = nullptr;
    //     vk::raii::DeviceMemory bufferMem = nullptr;
    //     createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
    //                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer,
    //                  bufferMem);

    //     computeUniformBuffers.emplace_back(std::move(buffer));
    //     computeUniformBuffersMemory.emplace_back(std::move(bufferMem));
    //     computeUniformBuffersMapped.emplace_back(computeUniformBuffersMemory.back().mapMemory(0, bufferSize));
    // }
}

void VulkanCompute::createComputeDescriptorPool() {
    // std::array poolSizes = {
    //     vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
    //     vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT * 2),
    // };

    // vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    //                                       .maxSets = MAX_FRAMES_IN_FLIGHT,
    //                                       .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
    //                                       .pPoolSizes = poolSizes.data()};

    // computeDescriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void VulkanCompute::createComputeDescriptorSetLayout() {
    // std::array layoutBindings = {
    //     vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr),
    //     vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr),
    //     vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr),
    // };

    // vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
    //                                              .pBindings = layoutBindings.data()};
    // computeDescriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void VulkanCompute::createComputeDescriptorSets() {
    // std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *computeDescriptorSetLayout);
    // vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = computeDescriptorPool,
    //                                       .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
    //                                       .pSetLayouts = layouts.data()};

    // computeDescriptorSets.clear();
    // computeDescriptorSets = device.allocateDescriptorSets(allocInfo);

    // for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    //     vk::DescriptorBufferInfo bufferInfo{.buffer = computeUniformBuffers[i],
    //                                        .offset = 0,
    //                                        .range = sizeof(UniformBufferObjectCompute)};

    //     vk::DescriptorBufferInfo storageBufferInfoLastFrame{
    //         .buffer = shaderStorageBuffers[(i - 1) % MAX_FRAMES_IN_FLIGHT],
    //         .offset = 0,
    //         .range = sizeof(Particle) * PARTICLE_COUNT,
    //     };
    //     vk::DescriptorBufferInfo storageBufferInfoCurrentFrame{
    //         .buffer = shaderStorageBuffers[i],
    //         .offset = 0,
    //         .range = sizeof(Particle) * PARTICLE_COUNT,
    //     };

    //     std::array descriptorWrites{
    //         vk::WriteDescriptorSet{.dstSet = computeDescriptorSets[i],
    //                                .dstBinding = 0,
    //                                .dstArrayElement = 0,
    //                                .descriptorCount = 1,
    //                                .descriptorType = vk::DescriptorType::eUniformBuffer,
    //                                .pBufferInfo = &bufferInfo},
    //         vk::WriteDescriptorSet{.dstSet = computeDescriptorSets[i],
    //                                .dstBinding = 1,
    //                                .dstArrayElement = 0,
    //                                .descriptorCount = 1,
    //                                .descriptorType = vk::DescriptorType::eStorageBuffer,
    //                                .pBufferInfo = &storageBufferInfoLastFrame},
    //         vk::WriteDescriptorSet{.dstSet = computeDescriptorSets[i],
    //                                .dstBinding = 2,
    //                                .dstArrayElement = 0,
    //                                .descriptorCount = 1,
    //                                .descriptorType = vk::DescriptorType::eStorageBuffer,
    //                                .pBufferInfo = &storageBufferInfoCurrentFrame},
    //     };
    //     device.updateDescriptorSets(descriptorWrites, {});
    // }
}

void VulkanCompute::createComputePipeline() {
    // auto computeShaderCode = readFile("shaders/compute.spv");
    // vk::raii::ShaderModule shaderModule = createShaderModule(computeShaderCode);

    // [[maybe_unused]] vk::PipelineShaderStageCreateInfo computeShaderStageInfo{.stage = vk::ShaderStageFlagBits::eCompute,
    //                                                                           .module = shaderModule,
    //                                                                           .pName = "compMain"};

    // vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1, .pSetLayouts = &*computeDescriptorSetLayout};
    // computePipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
    // vk::ComputePipelineCreateInfo pipelineInfo{.stage = computeShaderStageInfo, .layout = *computePipelineLayout};
    // computePipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

void VulkanCompute::createComputeCommandBuffers() {
    // computeCommandBuffers.clear();
    // vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
    //                                        .level = vk::CommandBufferLevel::ePrimary,
    //                                        .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
    // computeCommandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void VulkanCompute::recordComputeCommandBuffer(uint32_t frameIndex) {
    // auto& commandBuffer = computeCommandBuffers[frameIndex];
    // commandBuffer.reset();
    // commandBuffer.begin({});

    // commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
    // commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePipelineLayout, 0,
    //                                  {*computeDescriptorSets[frameIndex]}, {});

    // const uint32_t groupCount = (PARTICLE_COUNT + COMPUTE_WORKGROUP_SIZE - 1) / COMPUTE_WORKGROUP_SIZE;
    // commandBuffer.dispatch(groupCount, 1, 1);

    // commandBuffer.end();
}

void VulkanCompute::updateComputeUniformBuffer(uint32_t currentFrame) {
    // static auto startTime = std::chrono::high_resolution_clock::now();
    // static auto previousTime = startTime;

    // auto currentTime = std::chrono::high_resolution_clock::now();
    // float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - previousTime).count();
    // previousTime = currentTime;

    // // Tutorial uses frame time in ms * 2 with velocity ~0.00025; raw seconds are far too small to see.
    // UniformBufferObjectCompute ubo{};
    // ubo.deltaTime = deltaTime * 1000.0f * 2.0f;
    // std::memcpy(computeUniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

}