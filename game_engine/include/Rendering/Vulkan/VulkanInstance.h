#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

struct GLFWwindow;

namespace GameEngine {

class VulkanInstance {
public:
    VulkanInstance() = default;
    ~VulkanInstance() = default;

    void create(GLFWwindow* window);

    vk::raii::Context& getContext();
    vk::raii::Instance& getInstance();
    GLFWwindow* getWindow() const { return window_; }

private:
    void createInstance();
    std::vector<const char*> getRequiredInstanceExtensions() const;
    void setupDebugMessenger();

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    GLFWwindow* window_ = nullptr;
    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debugMessenger_{nullptr};
};

}
