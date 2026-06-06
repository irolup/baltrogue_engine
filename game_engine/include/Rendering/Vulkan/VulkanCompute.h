#include <cstdint>

namespace GameEngine {


class VulkanCompute {

public:
    void initCompute();

private:
    void createShaderStorageBuffers();
    void createComputeUniformBuffers();
    void createComputeDescriptorSetLayout();
    void createComputeDescriptorPool();
    void createComputeDescriptorSets();
    void createComputePipeline();
    void createComputeCommandBuffers();
    void recordComputeCommandBuffer(uint32_t frameIndex);
    void updateComputeUniformBuffer(uint32_t currentFrame);
};
}