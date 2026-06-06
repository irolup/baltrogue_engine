#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include "../Components/TextComponent.h"
namespace GameEngine {

class Mesh;
class Material;
class TextMaterial;

struct RenderCommand {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    glm::mat4 modelMatrix;
    glm::mat3 normalMatrix;
    std::vector<glm::mat4> boneTransforms;
    bool disableCulling = false;
    bool isBeam = false;
    glm::vec3 beamStart{0.0f};
    glm::vec3 beamEnd{0.0f, 0.0f, -1.0f};
    float beamHalfWidth = 0.04f;
};

struct TextRenderCommand {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<TextMaterial> material;
    glm::mat4 modelMatrix;
    glm::vec4 color;
    TextRenderMode renderMode;
    const TextComponent* textComponent = nullptr;
};

struct DeviceCapabilities {
        uint32_t apiVersion = 0;

        bool dynamicRendering = false;
        bool synchronization2 = false;
        bool timelineSemaphore = false;

        bool graphicsPipelineLibrary = false;
};

// Per-frame uniform layout (camera/projection) used by Vulkan
struct PerFrameUniforms {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPosition; // w unused
};

enum DescriptorSetIndex : uint32_t {
    SET_FRAME = 0,
    SET_MATERIAL = 1,
    SET_ANIMATION = 2
};

struct MaterialUniforms {
    glm::vec4 baseColor;
    float roughness;
    float metallic;
    float reflectionStrength;
    float padding; // pad to 16 byte alignment
    glm::vec4 textureFlags; // x diffuse, y normal, z arm, w environment
};

struct TextMaterialUniforms {
    glm::vec4 color;
};

struct AnimationUniforms {
    // match shader-side maximum, may be reduced later
    glm::mat4 boneMatrices[100];
};

// Push constants used per-draw
struct PushConstants {
    glm::mat4 modelMatrix;
    int objectID;
    int _pad0;
    int _pad1;
    int _pad2; // pad to 16 bytes
};

} 