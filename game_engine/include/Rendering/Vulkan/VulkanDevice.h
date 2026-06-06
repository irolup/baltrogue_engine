#pragma once

#include <vulkan/vulkan_raii.hpp>
#include "Rendering/RenderTypes.h"

struct GLFWwindow;

namespace GameEngine {

class VulkanInstance;
    
class VulkanDevice {
public:

    VulkanDevice() = default;
    ~VulkanDevice() = default;

    void create(VulkanInstance& instance);

    vk::raii::Instance& getVkInstance();
    vk::raii::PhysicalDevice& getPhysicalDevice();
    vk::raii::Device& getDevice();
    vk::raii::Queue& getQueue();
    vk::raii::SurfaceKHR& getSurface();
    vk::SampleCountFlagBits& getMsaaSamples();
    uint32_t& getQueueFamilyIndex();
    DeviceCapabilities& getDeviceCapabilities();
    GLFWwindow* getWindow();

private:

    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    bool checkDeviceExtensionSupport(vk::raii::PhysicalDevice const& device);
    bool hasExtension(::std::span<const vk::ExtensionProperties> extensions, const char* extensionName);
    bool hasGraphicsQueue(vk::raii::PhysicalDevice const& device);
    bool hasGraphicsComputePresentQueue(vk::raii::PhysicalDevice const& device) const;
    bool isDeviceSuitable(vk::raii::PhysicalDevice const& device);

    DeviceCapabilities queryDeviceCapabilities(vk::raii::PhysicalDevice const& device);
    int rateDevice(vk::raii::PhysicalDevice const& device);


    vk::SampleCountFlagBits getMaxUsableSampleCount();

    VulkanInstance* instance_ = nullptr;
    vk::raii::SurfaceKHR surface_{nullptr};
    vk::raii::PhysicalDevice physicalDevice_{nullptr};
    vk::raii::Device device_{nullptr};
    vk::raii::Queue queue_{nullptr};
    uint32_t queueIndex_ = ~0u;
    DeviceCapabilities deviceCapabilities_{};
    std::vector<const char*> requiredDeviceExtensions_;
    vk::SampleCountFlagBits msaaSamples_;


};

}