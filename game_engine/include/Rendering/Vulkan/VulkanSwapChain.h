#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace GameEngine {

class VulkanDevice;
class VulkanResources;

class VulkanSwapChain {
public:
    VulkanSwapChain() = default;
    ~VulkanSwapChain() = default;

    void create(VulkanDevice& device);
    void cleanupSwapChain();
    void recreateSwapChain(VulkanResources& vulkanResources);

    vk::raii::SwapchainKHR& getSwapChain();
    vk::Extent2D getExtent() const { return swapChainExtent_; }
    vk::SurfaceFormatKHR getSurfaceFormat() const { return swapChainSurfaceFormat_; }
    const std::vector<vk::Image>& getImages() const { return swapChainImages_; }
    const std::vector<vk::raii::ImageView>& getImageViews() const { return swapChainImageViews_; }

private:
    void createSwapChain();
    void createImageViews();

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) const;
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;
    uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) const;

    VulkanDevice* vulkanDevice_ = nullptr;

    vk::Extent2D swapChainExtent_{};
    vk::SurfaceFormatKHR swapChainSurfaceFormat_{};
    vk::raii::SwapchainKHR swapChain_{nullptr};
    std::vector<vk::Image> swapChainImages_;
    std::vector<vk::raii::ImageView> swapChainImageViews_;
};

}
