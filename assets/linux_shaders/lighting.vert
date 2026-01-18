#version 120

// Attributes (match VAO indices)
attribute vec3 position;  // 0
attribute vec3 normal;    // 1
attribute vec2 texCoord;  // 2
attribute vec3 tangent;   // 3
attribute vec4 boneWeights;  // 4
attribute vec4 boneIndices;  // 5

// Uniforms
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;

// Bone animation uniforms
uniform mat4 u_BoneMatrices[100];  // Maximum 100 bones
uniform int u_NumBones;

// Outputs to fragment shader
varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;
varying vec3 vViewPos;
varying vec3 vTangent;
varying vec3 vBitangent;

void main() {
    vec4 skinnedPosition = vec4(position, 1.0);
    vec3 skinnedNormal = normal;
    vec3 skinnedTangent = tangent;
    
    // Apply bone skinning if bones are available
    if (u_NumBones > 0 && boneWeights.x > 0.0) {
        // Get bone indices (convert from float to int)
        // GLSL 120/Cg: Use ivec4 to convert all at once, then extract components
        ivec4 boneIndicesInt = ivec4(floor(boneIndices + 0.5));
        int boneIndex0 = boneIndicesInt.x;
        int boneIndex1 = boneIndicesInt.y;
        int boneIndex2 = boneIndicesInt.z;
        int boneIndex3 = boneIndicesInt.w;
        
        // Clamp bone indices to valid range (max 99 to match array size)
        int maxBoneIndex = u_NumBones - 1;
        if (maxBoneIndex > 99) maxBoneIndex = 99;
        // Manual clamping for GLSL 120 compatibility (clamp() may return float)
        if (boneIndex0 < 0) boneIndex0 = 0;
        if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
        if (boneIndex1 < 0) boneIndex1 = 0;
        if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
        if (boneIndex2 < 0) boneIndex2 = 0;
        if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
        if (boneIndex3 < 0) boneIndex3 = 0;
        if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
        
        // Apply bone transformations (blend matrices correctly)
        // Each bone transform is applied separately and then blended
        vec4 pos0 = u_BoneMatrices[boneIndex0] * vec4(position, 1.0);
        vec4 pos1 = u_BoneMatrices[boneIndex1] * vec4(position, 1.0);
        vec4 pos2 = u_BoneMatrices[boneIndex2] * vec4(position, 1.0);
        vec4 pos3 = u_BoneMatrices[boneIndex3] * vec4(position, 1.0);
        
        // Blend the transformed positions
        skinnedPosition = pos0 * boneWeights.x + pos1 * boneWeights.y + pos2 * boneWeights.z + pos3 * boneWeights.w;
        
        // Transform normal and tangent (blend separately)
        vec3 norm0 = normalize(mat3(u_BoneMatrices[boneIndex0]) * normal);
        vec3 norm1 = normalize(mat3(u_BoneMatrices[boneIndex1]) * normal);
        vec3 norm2 = normalize(mat3(u_BoneMatrices[boneIndex2]) * normal);
        vec3 norm3 = normalize(mat3(u_BoneMatrices[boneIndex3]) * normal);
        skinnedNormal = normalize(norm0 * boneWeights.x + norm1 * boneWeights.y + norm2 * boneWeights.z + norm3 * boneWeights.w);
        
        vec3 tan0 = normalize(mat3(u_BoneMatrices[boneIndex0]) * tangent);
        vec3 tan1 = normalize(mat3(u_BoneMatrices[boneIndex1]) * tangent);
        vec3 tan2 = normalize(mat3(u_BoneMatrices[boneIndex2]) * tangent);
        vec3 tan3 = normalize(mat3(u_BoneMatrices[boneIndex3]) * tangent);
        skinnedTangent = normalize(tan0 * boneWeights.x + tan1 * boneWeights.y + tan2 * boneWeights.z + tan3 * boneWeights.w);
    }
    
    // Apply model matrix to skinned position (matching LearnOpenGL: model * view * projection after bone transform)
    // Bone matrices are in model space, so we need to apply modelMatrix after bone transformation
    vec4 worldPos = modelMatrix * skinnedPosition;
    vWorldPos = worldPos.xyz;

    // View position
    vec4 viewPos = viewMatrix * worldPos;
    vViewPos = viewPos.xyz;

    // Transform normal to world space
    vNormal = normalize(normalMatrix * skinnedNormal);

    // Transform tangent to world space
    vTangent = normalize(normalMatrix * skinnedTangent);

    // Compute bitangent
    vBitangent = normalize(cross(vNormal, vTangent));

    // Texture coordinate (flip Y to match Vita)
    vTexCoord = vec2(texCoord.x, 1.0 - texCoord.y);

    // Clip-space position
    gl_Position = projectionMatrix * viewPos;
}