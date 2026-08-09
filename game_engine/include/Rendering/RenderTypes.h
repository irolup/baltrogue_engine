#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include "../Components/TextComponent.h"
#include "Rendering/ShadowMap.h"
namespace GameEngine {

class Mesh;
class Material;
class TextMaterial;

struct RenderCommand {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    glm::mat4 modelMatrix;
    glm::mat3 normalMatrix;
    std::shared_ptr<const std::vector<glm::mat4>> boneTransforms;
    bool disableCulling = false;
    bool isBeam = false;
    bool castShadows = true;
    bool receiveShadows = true;
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
    int32_t numShadowViews; // offset 152, 0 disables every shadow lookup
    int32_t _pad2; // offset 156 (std140 alignment)
    std::array<GpuLight, 16> lights; //Offset 160 needed to be a multiple of 8 for array for std140
    glm::vec4 shadowParams; // offset 1440: 1/atlasWidth, 1/atlasHeight, soft filter flag
    std::array<glm::mat4, kMaxShadowViews> shadowMatrices; // offset 1456
};

enum DescriptorSetIndex : uint32_t {
    SET_FRAME = 0,
    SET_MATERIAL = 1,
    SET_ENVIRONMENT = 2,
    SET_ANIMATION = 3
};

static const uint32_t kMaxBones = 100;
static const uint32_t kMaxSkinnedDrawsPerFrame = 128;
static const uint32_t kInvalidAnimationSlot = UINT32_MAX;

struct FrameEnvironment {
    bool active = false;
    const void* cacheKey = nullptr;
};

struct MaterialUniforms {
    glm::vec4 baseColor;
    float roughness;
    float metallic;
    float reflectionStrength;
    float alphaCutoff; // >0 enables alpha-test discard
    glm::vec4 textureFlags; // x diffuse, y normal, z arm, w environment
    glm::vec4 uvScaleOffset; // xy = scale, zw = offset
};

struct TextMaterialUniforms {
    glm::vec4 color;
};

struct AnimationUniforms {
    int32_t numBones = 0;
    int32_t _pad0 = 0;
    int32_t _pad1 = 0;
    int32_t _pad2 = 0;
    std::array<glm::mat4, kMaxBones> boneMatrices{};
};

// Push constants used per-draw
struct PushConstants {
    glm::mat4 modelMatrix;
    int objectID;
    int receiveShadows; // 0 skips every shadow lookup for this draw
    int shadowViewIndex; // shadow pass only: which atlas tile is being filled
    int _pad2;           // pad to 16 bytes
};

struct BeamPushConstants {
    glm::vec4 beamStart;
    glm::vec4 beamEnd;
    float beamHalfWidth = 0.04f;
    float time = 0.0f;
    float _pad0 = 0.0f;
    float _pad1 = 0.0f;
};

struct ShaderMaterialUniforms {
    glm::vec4 baseColor;
    glm::vec4 textureFlags; // x=diffuse, y=custom0, z=custom1, w=custom2
};

enum class VulkanShaderPipelineKind {
    DefaultLit,
    Beam,
    Custom
};

} 