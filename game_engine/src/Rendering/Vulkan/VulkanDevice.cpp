#include "Rendering/Vulkan/VulkanDevice.h"
#include "Rendering/Vulkan/VulkanInstance.h"
#include <cstring>
#include <map>
#include <ranges>
#include <stdexcept>
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace GameEngine {


void VulkanDevice::create(VulkanInstance& instance){
    assert(instance.getWindow() != nullptr && "VulkanInstance window is null");

    instance_ = &instance;
    createSurface();
    pickPhysicalDevice();
    msaaSamples_ = getMaxUsableSampleCount();
    createLogicalDevice();
}

void VulkanDevice::createSurface() {
    GLFWwindow* window = instance_->getWindow();
    vk::raii::Instance& inst = instance_->getInstance();

    VkSurfaceKHR raw{};


    if (glfwCreateWindowSurface(static_cast<VkInstance>(*inst), window, nullptr, &raw) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    surface_ = vk::raii::SurfaceKHR(inst, raw);
}

vk::SampleCountFlagBits VulkanDevice::getMaxUsableSampleCount() {
    vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice_.getProperties();

    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

    return vk::SampleCountFlagBits::e1;
}

void VulkanDevice::pickPhysicalDevice() {
    auto physicalDevices = vk::raii::PhysicalDevices(instance_->getInstance());

    if (physicalDevices.empty()) {
        throw std::runtime_error("No Vulkan devices found!");
    }

    std::multimap<int, vk::raii::PhysicalDevice> candidates;

    for (const auto& device : physicalDevices) {
        if (!isDeviceSuitable(device)) {
            continue;
        }

        int score = rateDevice(device);
        candidates.insert({score, device});
    }

    if (!candidates.empty()) {
        physicalDevice_ = candidates.rbegin()->second;
        deviceCapabilities_ = queryDeviceCapabilities(physicalDevice_);
    } else {
        throw std::runtime_error("No suitable GPU found!");
    }
}

bool VulkanDevice::checkDeviceExtensionSupport(vk::raii::PhysicalDevice const& device) {
    auto extensions =
        device.enumerateDeviceExtensionProperties();

    auto props = device.getProperties();

    std::vector<const char*> requiredExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    if (props.apiVersion < VK_API_VERSION_1_3)
    {
        requiredExtensions.push_back(
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

        requiredExtensions.push_back(
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }

    if (props.apiVersion < VK_API_VERSION_1_2)
    {
        requiredExtensions.push_back(
            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    }

    return std::ranges::all_of(
        requiredExtensions,
        [&extensions, this](const char* required)
        {
            return hasExtension(extensions, required);
        });
}

bool VulkanDevice::hasExtension(std::span<const vk::ExtensionProperties> extensions, const char* extensionName)
{
    return std::ranges::any_of(
        extensions,
        [extensionName](const auto& ext)
        {
            return strcmp(ext.extensionName, extensionName) == 0;
        });
}

bool VulkanDevice::hasGraphicsQueue(vk::raii::PhysicalDevice const& device) {
    auto queueFamilies = device.getQueueFamilyProperties();

    return std::ranges::any_of(queueFamilies, [](auto const& qfp) {
        return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
    });
}

bool VulkanDevice::hasGraphicsComputePresentQueue(vk::raii::PhysicalDevice const& device) const {
    auto queueFamilies = device.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        auto const& flags = queueFamilies[i].queueFlags;
        if ((flags & vk::QueueFlagBits::eGraphics) && (flags & vk::QueueFlagBits::eCompute) &&
            device.getSurfaceSupportKHR(i, *surface_)) {
            return true;
        }
    }
    return false;
}

DeviceCapabilities VulkanDevice::queryDeviceCapabilities(vk::raii::PhysicalDevice const& device) {
    DeviceCapabilities caps{};

    auto props = device.getProperties();
    caps.apiVersion = props.apiVersion;

    auto extensions = device.enumerateDeviceExtensionProperties();

    caps.dynamicRendering = props.apiVersion >= VK_API_VERSION_1_3 ||
                            hasExtension(extensions, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

    caps.synchronization2 = props.apiVersion >= VK_API_VERSION_1_3 ||
                            hasExtension(extensions, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

    caps.timelineSemaphore = props.apiVersion >= VK_API_VERSION_1_2 ||
                             hasExtension(extensions, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

    caps.graphicsPipelineLibrary = hasExtension(extensions, VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);

    return caps;
}

bool VulkanDevice::isDeviceSuitable(vk::raii::PhysicalDevice const& device) {
    auto props = device.getProperties();

    if (props.apiVersion < VK_API_VERSION_1_2) {
        return false;
    }

    if (!checkDeviceExtensionSupport(device)) {
        return false;
    }

    if (!hasGraphicsQueue(device)) {
        return false;
    }

    if (!hasGraphicsComputePresentQueue(device)) {
        return false;
    }

    auto caps = queryDeviceCapabilities(device);

    if (!caps.dynamicRendering) {
        return false;
    }

    if (!caps.synchronization2) {
        return false;
    }

    if (!caps.timelineSemaphore) {
        return false;
    }

    auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                                                  vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
                                                  vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

    auto const& core = features.get<vk::PhysicalDeviceFeatures2>();
    auto const& v11 = features.get<vk::PhysicalDeviceVulkan11Features>();
    auto const& ext = features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

    if (!core.features.samplerAnisotropy) {
        return false;
    }

    if (!v11.shaderDrawParameters) {
        return false;
    }

    if (!ext.extendedDynamicState) {
        return false;
    }

    return true;
}



int VulkanDevice::rateDevice(vk::raii::PhysicalDevice const& device) {
    auto properties = device.getProperties();
    auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                                                  vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
                                                  vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

    int score = 0;

    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1000;
    }

    score += properties.limits.maxImageDimension2D;

    auto const& core = features.get<vk::PhysicalDeviceFeatures2>().features;
    auto const& ext = features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

    if (properties.apiVersion >= vk::ApiVersion13) {
        score += 200;
    }

    if (ext.extendedDynamicState) {
        score += 100;
    }

    if (core.samplerAnisotropy) {
        score += 100;
    }

    return score;
}

void VulkanDevice::createLogicalDevice() {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice_.getQueueFamilyProperties();

    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
        auto const& flags = queueFamilyProperties[qfpIndex].queueFlags;
        if ((flags & vk::QueueFlagBits::eGraphics) && (flags & vk::QueueFlagBits::eCompute) &&
            physicalDevice_.getSurfaceSupportKHR(qfpIndex, *surface_)) {
            queueIndex_ = qfpIndex;
            break;
        }
    }
    if (queueIndex_ == ~0u) {
        throw std::runtime_error("Could not find a queue for graphics, compute, and present -> terminating");
    }

    requiredDeviceExtensions_.clear();
    requiredDeviceExtensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    if (deviceCapabilities_.apiVersion < VK_API_VERSION_1_3) {
        requiredDeviceExtensions_.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        requiredDeviceExtensions_.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }

    if (deviceCapabilities_.apiVersion < VK_API_VERSION_1_2) {
        requiredDeviceExtensions_.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan12Features,
                       vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain{
            vk::PhysicalDeviceFeatures2{}.setFeatures(vk::PhysicalDeviceFeatures{}.setSamplerAnisotropy(true)),
                vk::PhysicalDeviceVulkan11Features{}.setShaderDrawParameters(true),
                vk::PhysicalDeviceVulkan12Features{}.setTimelineSemaphore(true),
            vk::PhysicalDeviceVulkan13Features{}.setSynchronization2(true).setDynamicRendering(true),
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{}.setExtendedDynamicState(true),
        };

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
    deviceQueueCreateInfo.setQueueFamilyIndex(queueIndex_).setQueueCount(1).setPQueuePriorities(&queuePriority);
    vk::DeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>());
    deviceCreateInfo.setQueueCreateInfoCount(1);
    deviceCreateInfo.setPQueueCreateInfos(&deviceQueueCreateInfo);
    deviceCreateInfo.setEnabledExtensionCount(static_cast<uint32_t>(requiredDeviceExtensions_.size()));
    deviceCreateInfo.setPpEnabledExtensionNames(requiredDeviceExtensions_.data());

    device_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
    queue_ = vk::raii::Queue(device_, queueIndex_, 0);
}

vk::raii::Instance& VulkanDevice::getVkInstance(){
    return instance_->getInstance();
}

vk::raii::PhysicalDevice& VulkanDevice::getPhysicalDevice(){
    return physicalDevice_;
}

vk::raii::Device& VulkanDevice::getDevice(){
    return device_;
}

vk::raii::Queue& VulkanDevice::getQueue(){
    return queue_;
}

vk::raii::SurfaceKHR& VulkanDevice::getSurface(){
    return surface_;
}

vk::SampleCountFlagBits& VulkanDevice::getMsaaSamples(){
    return msaaSamples_;
}

uint32_t& VulkanDevice::getQueueFamilyIndex(){
    return queueIndex_;
}

DeviceCapabilities& VulkanDevice::getDeviceCapabilities(){
    return deviceCapabilities_;
}

GLFWwindow* VulkanDevice::getWindow(){
    return instance_->getWindow();
}

}