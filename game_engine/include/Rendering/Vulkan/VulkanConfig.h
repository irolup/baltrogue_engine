#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace GameEngine {

inline constexpr char const* VULKAN_APP_NAME = "Baltrogue";
inline constexpr char const* VULKAN_ENGINE_NAME = "BaltrogueEngine";

#ifdef ENABLE_VULKAN
inline glm::mat4 fixProjectionForVulkan(glm::mat4 projection) {
    projection[1][1] *= -1.0f;
    return projection;
}
#endif

#ifdef NDEBUG
inline constexpr bool kEnableValidationLayers = false;
#else
inline constexpr bool kEnableValidationLayers = true;
#endif

inline const std::vector<char const*> kValidationLayers = {
  "VK_LAYER_KHRONOS_validation",
};

}