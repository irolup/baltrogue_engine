#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inBoneWeights;
layout(location = 5) in vec4 inBoneIndices;

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
} uFrame;

layout(push_constant) uniform PushConstants {
    mat4 model;
} uPushConstants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out vec3 outWorldTangent;

void main() {
    vec4 worldPos = uPushConstants.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(uPushConstants.model)));

    outTexCoord = inTexCoord;
    outWorldPos = worldPos.xyz;
    outNormal = normalize(normalMatrix * inNormal);
    outWorldTangent = normalize(normalMatrix * inTangent);
    gl_Position = uFrame.proj * uFrame.view * worldPos;
}