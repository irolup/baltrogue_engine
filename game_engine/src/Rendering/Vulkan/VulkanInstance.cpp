// PLATFORM: Vulkan instance / validation — device profiles & portability live here too.
#include "Rendering/Vulkan/VulkanConfig.h"

#include "Rendering/Vulkan/VulkanInstance.h"

#include <algorithm>
#include <cstring>
#include <ranges>
#include <iostream>
#include <stdexcept>
#include <string>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace GameEngine {

void VulkanInstance::create(GLFWwindow* window)
{
    window_ = window;
    createInstance();
    setupDebugMessenger();
}

void VulkanInstance::createInstance() {
    constexpr vk::ApplicationInfo appInfo{VULKAN_APP_NAME,
                                         VK_MAKE_VERSION(1, 0, 0),
                                         VULKAN_ENGINE_NAME,
                                         VK_MAKE_VERSION(1, 0, 0),
                                         VK_API_VERSION_1_4};

    std::vector<char const*> requiredLayers;
    if (kEnableValidationLayers) {
        requiredLayers.assign(kValidationLayers.begin(), kValidationLayers.end());
    }

    auto layerProperties = context_.enumerateInstanceLayerProperties();
    auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const& requiredLayer) {
        return std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty) {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
    });

    if (unsupportedLayerIt != requiredLayers.end()) {
        throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    }

    auto requiredExtensions = getRequiredInstanceExtensions();

    auto extensionProperties = context_.enumerateInstanceExtensionProperties();
    auto unsupportedPropertyIt = std::ranges::find_if(requiredExtensions, [&extensionProperties](auto const& requiredExtension) {
        return std::ranges::none_of(extensionProperties, [requiredExtension](auto const& extensionProperty) {
            return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
        });
    });
    if (unsupportedPropertyIt != requiredExtensions.end()) {
        throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
    }

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo= &appInfo;
    createInfo.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size());
    createInfo.ppEnabledLayerNames     = requiredLayers.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    // vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    // if (kEnableValidationLayers) {
    //     debugCreateInfo.messageSeverity =
    //         vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
    //         vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
    //         vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
    //         vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    //     debugCreateInfo.messageType =
    //         vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
    //         vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
    //         vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    //     debugCreateInfo.pfnUserCallback = &debugCallback;
    //     createInfo.pNext = &debugCreateInfo;
    // }

    instance_ = vk::raii::Instance(context_, createInfo);
}

std::vector<const char*> VulkanInstance::getRequiredInstanceExtensions() const {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (kEnableValidationLayers) {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}

void VulkanInstance::setupDebugMessenger() {
    if (!kEnableValidationLayers) {
        return;
    }

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{};
    debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
    debugUtilsMessengerCreateInfoEXT.messageType = messageTypeFlags;
    debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &debugCallback;

    debugMessenger_ = instance_.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanInstance::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT /*severity*/,
                                                                         vk::DebugUtilsMessageTypeFlagsEXT type,
                                                                         const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                                         void* /*pUserData*/) {
    std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
    return vk::False;
}

vk::raii::Context& VulkanInstance::getContext(){
    return context_;
}

vk::raii::Instance& VulkanInstance::getInstance(){
    return instance_;
}

}