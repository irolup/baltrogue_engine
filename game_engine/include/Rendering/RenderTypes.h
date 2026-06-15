#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
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

// std140: each Light = 5 x vec4 = 80 bytes
struct GpuLight {
    glm::vec4 position; // w = light type (0=dir, 1=point, 2=spot)
    glm::vec4 direction; // w = intensity
    glm::vec4 color; // w = range
    glm::vec4 params; // cutOff, outerCutOff, constant, linear
    glm::vec4 attenuation;// x = quadratic
};

// Need aligment cause we use std140
struct PerFrameUniforms {
    glm::mat4 view; //Offset 0
    glm::mat4 proj; //Offset 64
    glm::vec4 cameraPosition; //Offset 128 
    int32_t numLights; // offset 144
    int32_t hasEnvironmentMap; // offset 148, 1 when active skybox cubemap is bound
    int32_t _pad1, _pad2; // offset 152–156 (std140 alignment)
    std::array<GpuLight, 16> lights; //Offset 160 needed to be a multiple of 8 for array for std140
};

enum DescriptorSetIndex : uint32_t {
    SET_FRAME = 0,
    SET_MATERIAL = 1,
    SET_ENVIRONMENT = 2,
    SET_ANIMATION = 3
};

struct FrameEnvironment {
    bool active = false;
    std::string cacheKey;
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