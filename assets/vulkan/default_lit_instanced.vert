#version 450

// Instanced variant of default_lit.vert.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(location = 6) in vec4 inModelCol0;
layout(location = 7) in vec4 inModelCol1;
layout(location = 8) in vec4 inModelCol2;
layout(location = 9) in vec4 inModelCol3;
layout(location = 10) in vec4 inNormalCol0;
layout(location = 11) in vec4 inNormalCol1;
layout(location = 12) in vec4 inNormalCol2;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 attenuation;
};

layout(std140, set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    int numLights;
    int hasEnvironmentMap;
    int numShadowViews;
    int _pad2;
    Light lights[16];
    vec4 shadowParams;
    mat4 shadowMatrices[8];
} uFrame;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int objectID;
    int receiveShadows;
    int shadowViewIndex;
    int _pad2;
} uPushConstants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out vec3 outWorldTangent;

void main() {
    mat4 model = mat4(inModelCol0, inModelCol1, inModelCol2, inModelCol3);
    mat3 normalMatrix = mat3(inNormalCol0.xyz, inNormalCol1.xyz, inNormalCol2.xyz);

    vec4 worldPos = model * vec4(inPosition, 1.0);

    outTexCoord = inTexCoord;
    outWorldPos = worldPos.xyz;
    outNormal = normalize(normalMatrix * inNormal);
    outWorldTangent = normalize(normalMatrix * inTangent);
    gl_Position = uFrame.proj * uFrame.view * worldPos;
}
