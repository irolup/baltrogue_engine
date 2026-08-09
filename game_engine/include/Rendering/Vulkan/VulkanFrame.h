#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "Rendering/Vulkan/VulkanDevice.h"
#include "Rendering/Vulkan/VulkanPipeline.h"
#include "Rendering/Vulkan/VulkanResources.h"

namespace GameEngine {

class IRenderer;
class Scene;
class VulkanRenderer;

class VulkanFrame {
public:
    void create(VulkanDevice& device, VulkanResources& resources, VulkanSwapChain& swapChain, VulkanPipeline& pipeline);

    void setScene(Scene* scene) { scene_ = scene; }
    void setRenderer(IRenderer* renderer) { renderer_ = renderer; }
    void setVulkanRenderer(VulkanRenderer* renderer) { vulkanRenderer_ = renderer; }

    void waitUntilIdle();

    void drawFrame();

private:
    void createSyncObjects();
    void createSwapchainSyncObjects();
    void renderScene();

    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Fence> inFlightFences_;
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<VkSemaphore> presentWaitSemaphores_;
    std::vector<vk::Fence> imagesInFlight_;
    std::vector<bool> swapchainImagePresented_;

    uint32_t frameIndex_ = 0;
    uint64_t timelineValue = 0;
    bool framebufferResized = false;

    VulkanDevice* device_ = nullptr;
    VulkanResources* resources_ = nullptr;
    VulkanSwapChain* swapChain_ = nullptr;
    VulkanPipeline* pipeline_ = nullptr;
    Scene* scene_ = nullptr;
    IRenderer* renderer_ = nullptr;
    VulkanRenderer* vulkanRenderer_ = nullptr;
};

} // namespace GameEngine