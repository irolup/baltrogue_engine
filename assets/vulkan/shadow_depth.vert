#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inBoneWeights;
layout(location = 5) in vec4 inBoneIndices;

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

layout(std140, set = 3, binding = 0) uniform AnimationUniforms {
    int numBones;
    int _pad0, _pad1, _pad2;
    mat4 boneMatrices[100];
} uAnimation;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int objectID;
    int receiveShadows;
    int shadowViewIndex;
    int _pad2;
} uPushConstants;

void main() {
    vec4 skinnedPosition = vec4(inPosition, 1.0);

    if (uAnimation.numBones > 0 && inBoneWeights.x > 0.0) {
        ivec4 boneIndicesInt = ivec4(floor(inBoneIndices + 0.5));
        int maxBoneIndex = min(uAnimation.numBones - 1, 99);

        int boneIndex0 = clamp(boneIndicesInt.x, 0, maxBoneIndex);
        int boneIndex1 = clamp(boneIndicesInt.y, 0, maxBoneIndex);
        int boneIndex2 = clamp(boneIndicesInt.z, 0, maxBoneIndex);
        int boneIndex3 = clamp(boneIndicesInt.w, 0, maxBoneIndex);

        vec4 pos0 = uAnimation.boneMatrices[boneIndex0] * vec4(inPosition, 1.0);
        vec4 pos1 = uAnimation.boneMatrices[boneIndex1] * vec4(inPosition, 1.0);
        vec4 pos2 = uAnimation.boneMatrices[boneIndex2] * vec4(inPosition, 1.0);
        vec4 pos3 = uAnimation.boneMatrices[boneIndex3] * vec4(inPosition, 1.0);
        skinnedPosition = pos0 * inBoneWeights.x + pos1 * inBoneWeights.y
                        + pos2 * inBoneWeights.z + pos3 * inBoneWeights.w;
    }

    int view = clamp(uPushConstants.shadowViewIndex, 0, 7);
    gl_Position = uFrame.shadowMatrices[view] * (uPushConstants.model * skinnedPosition);
}
