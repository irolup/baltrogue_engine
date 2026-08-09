#include "Rendering/Vulkan/VulkanSwapChain.h"
#include "Rendering/Vulkan/VulkanDevice.h"
#include "Rendering/Vulkan/VulkanResources.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace GameEngine {

void VulkanSwapChain::create(VulkanDevice& device) {
    vulkanDevice_ = &device;
    createSwapChain();
    createImageViews();
}

void VulkanSwapChain::createSwapChain() {
    if (!vulkanDevice_) {
        throw std::runtime_error("VulkanSwapChain::createSwapChain called before create()");
    }

    vk::raii::PhysicalDevice& physicalDevice = vulkanDevice_->getPhysicalDevice();
    vk::raii::SurfaceKHR& surface = vulkanDevice_->getSurface();
    vk::raii::Device& device = vulkanDevice_->getDevice();

    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    swapChainExtent_ = chooseSwapExtent(surfaceCapabilities);
    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
    swapChainSurfaceFormat_ = chooseSwapSurfaceFormat(availableFormats);

    std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.minImageCount = minImageCount;
    swapChainCreateInfo.imageFormat = swapChainSurfaceFormat_.format;
    swapChainCreateInfo.imageColorSpace = swapChainSurfaceFormat_.colorSpace;
    swapChainCreateInfo.imageExtent = swapChainExtent_;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = true;

    swapChain_ = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages_ = swapChain_.getImages();
}

void VulkanSwapChain::cleanupSwapChain() {
    swapChainImageViews_.clear();
    swapChainImages_.clear();
    swapChain_ = nullptr;
}

void VulkanSwapChain::recreateSwapChain(VulkanResources& vulkanResources) {
    if (!vulkanDevice_) {
        throw std::runtime_error("VulkanSwapChain::recreateSwapChain called before create()");
    }

    GLFWwindow* window = vulkanDevice_->getWindow();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vulkanDevice_->getDevice().waitIdle();

    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    vulkanResources.createDepthResources();
}

vk::SurfaceFormatKHR VulkanSwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format) {
        return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR VulkanSwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) const {
    assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
    return std::ranges::any_of(availablePresentModes,
                               [](vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; })
               ? vk::PresentModeKHR::eMailbox
               : vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanSwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(vulkanDevice_->getWindow(), &width, &height);

    return {
        std::clamp<uint32_t>(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}

uint32_t VulkanSwapChain::chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) const {
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}

void VulkanSwapChain::createImageViews() {
    assert(swapChainImageViews_.empty());

    auto& device = vulkanDevice_->getDevice();

    vk::ImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = swapChainSurfaceFormat_.format;
    imageViewCreateInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    for (auto& image : swapChainImages_) {
        imageViewCreateInfo.image = image;
        swapChainImageViews_.emplace_back(device, imageViewCreateInfo);
    }
}

vk::raii::SwapchainKHR& VulkanSwapChain::getSwapChain() {
    return swapChain_;
}

}
