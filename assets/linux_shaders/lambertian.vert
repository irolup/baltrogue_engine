#version 120

// Attributes
attribute vec3 position;
attribute vec3 normal;
// Optional bone attributes (if mesh has skinning data)
attribute vec4 boneWeights;
attribute vec4 boneIndices;

// Uniforms
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;
uniform vec3 pointLightPosition;

// Bone animation uniforms
uniform mat4 u_BoneMatrices[100];  // Maximum 100 bones
uniform int u_NumBones;

// Varyings (outputs to fragment shader)
varying vec3 lightDir;
varying vec3 vNormal;
varying vec3 vViewPosition;

void main() {
    vec4 skinnedPosition = vec4(position, 1.0);
    vec3 skinnedNormal = normal;
    
    // Apply bone skinning if bones are available
    if (u_NumBones > 0) {
        // Get bone indices (convert from float to int)
        int boneIndex0 = int(boneIndices.x);
        int boneIndex1 = int(boneIndices.y);
        int boneIndex2 = int(boneIndices.z);
        int boneIndex3 = int(boneIndices.w);
        
        // Clamp bone indices to valid range
        boneIndex0 = clamp(boneIndex0, 0, u_NumBones - 1);
        boneIndex1 = clamp(boneIndex1, 0, u_NumBones - 1);
        boneIndex2 = clamp(boneIndex2, 0, u_NumBones - 1);
        boneIndex3 = clamp(boneIndex3, 0, u_NumBones - 1);
        
        // Apply bone transformations
        mat4 boneTransform = 
            u_BoneMatrices[boneIndex0] * boneWeights.x +
            u_BoneMatrices[boneIndex1] * boneWeights.y +
            u_BoneMatrices[boneIndex2] * boneWeights.z +
            u_BoneMatrices[boneIndex3] * boneWeights.w;
        
        // Transform position and normal
        skinnedPosition = boneTransform * vec4(position, 1.0);
        
        // Transform normal (use 3x3 part of bone transform)
        mat3 boneNormalMatrix = mat3(boneTransform);
        skinnedNormal = normalize(boneNormalMatrix * normal);
    }
    
    // Calculating vertex position in modelview coordinate
    vec4 mvPosition = viewMatrix * modelMatrix * skinnedPosition;
    
    // View direction
    vViewPosition = -mvPosition.xyz;
    
    // Applying transformations to normals
    vNormal = normalize(normalMatrix * skinnedNormal);
    
    // Calculating light incidence direction
    vec4 lightPos = viewMatrix * vec4(pointLightPosition, 1.0);
    lightDir = lightPos.xyz - mvPosition.xyz;
    
    // Calculating final position in clip space
    gl_Position = projectionMatrix * mvPosition;
}
