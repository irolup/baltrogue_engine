#version 120
// GLSL Vertex Shader
// Custom Unlit Shader

attribute vec3 position;
attribute vec3 normal;
attribute vec2 texCoord;
attribute vec3 tangent;
attribute vec4 boneWeights;
attribute vec4 boneIndices;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform mat4 u_BoneMatrices[100];
uniform int u_NumBones;

varying vec2 vTexCoord;

void main() {
    vec4 skinnedPosition = vec4(position, 1.0);
    
    if (u_NumBones > 0 && boneWeights.x > 0.0) {
        ivec4 boneIndicesInt = ivec4(floor(boneIndices + 0.5));
        int boneIndex0 = boneIndicesInt.x;
        int boneIndex1 = boneIndicesInt.y;
        int boneIndex2 = boneIndicesInt.z;
        int boneIndex3 = boneIndicesInt.w;
        
        int maxBoneIndex = u_NumBones - 1;
        if (maxBoneIndex > 99) maxBoneIndex = 99;
        if (boneIndex0 < 0) boneIndex0 = 0;
        if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
        if (boneIndex1 < 0) boneIndex1 = 0;
        if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
        if (boneIndex2 < 0) boneIndex2 = 0;
        if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
        if (boneIndex3 < 0) boneIndex3 = 0;
        if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
        
        vec4 pos0 = u_BoneMatrices[boneIndex0] * vec4(position, 1.0);
        vec4 pos1 = u_BoneMatrices[boneIndex1] * vec4(position, 1.0);
        vec4 pos2 = u_BoneMatrices[boneIndex2] * vec4(position, 1.0);
        vec4 pos3 = u_BoneMatrices[boneIndex3] * vec4(position, 1.0);
        
        skinnedPosition = pos0 * boneWeights.x + pos1 * boneWeights.y + pos2 * boneWeights.z + pos3 * boneWeights.w;
    }
    
    vec4 worldPos = modelMatrix * skinnedPosition;
    vec4 viewPos = viewMatrix * worldPos;
    
    vTexCoord = vec2(texCoord.x, 1.0 - texCoord.y);
    gl_Position = projectionMatrix * viewPos;
}