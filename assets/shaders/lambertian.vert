// CG Vertex Shader for PS Vita (VitaGL)
// Lambertian lighting model

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 boneWeights : TEXCOORD3;  // Bone weights (optional)
    float4 boneIndices : TEXCOORD4; // Bone indices (optional)
};

struct VertexOutput {
    float4 position : POSITION;
    float3 lightDir : TEXCOORD0;
    float3 vNormal : TEXCOORD1;
    float3 vViewPosition : TEXCOORD2;
};

// Uniforms
uniform float4x4 modelMatrix;
uniform float4x4 viewMatrix;
uniform float4x4 projectionMatrix;
uniform float3x3 normalMatrix;
uniform float3 pointLightPosition;

// Bone animation uniforms
uniform float4x4 u_BoneMatrices[100];  // Maximum 100 bones
uniform int u_NumBones;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    float4 skinnedPosition = float4(input.position, 1.0f);
    float3 skinnedNormal = input.normal;
    
    // Apply bone skinning if bones are available
    if (u_NumBones > 0) {
        // Get bone indices (convert from float to int)
        int boneIndex0 = (int)input.boneIndices.x;
        int boneIndex1 = (int)input.boneIndices.y;
        int boneIndex2 = (int)input.boneIndices.z;
        int boneIndex3 = (int)input.boneIndices.w;
        
        // Clamp bone indices to valid range
        boneIndex0 = clamp(boneIndex0, 0, u_NumBones - 1);
        boneIndex1 = clamp(boneIndex1, 0, u_NumBones - 1);
        boneIndex2 = clamp(boneIndex2, 0, u_NumBones - 1);
        boneIndex3 = clamp(boneIndex3, 0, u_NumBones - 1);
        
        // Apply bone transformations
        float4x4 boneTransform = 
            u_BoneMatrices[boneIndex0] * input.boneWeights.x +
            u_BoneMatrices[boneIndex1] * input.boneWeights.y +
            u_BoneMatrices[boneIndex2] * input.boneWeights.z +
            u_BoneMatrices[boneIndex3] * input.boneWeights.w;
        
        // Transform position
        skinnedPosition = mul(float4(input.position, 1.0f), boneTransform);
        
        // Transform normal (use 3x3 part of bone transform)
        float3x3 boneNormalMatrix = float3x3(
            boneTransform[0].xyz,
            boneTransform[1].xyz,
            boneTransform[2].xyz
        );
        skinnedNormal = normalize(mul(input.normal, boneNormalMatrix));
    }
    
    // Calculate vertex position in modelview coordinate
    float4 mvPosition = mul(mul(skinnedPosition, modelMatrix), viewMatrix);
    
    // View direction
    output.vViewPosition = mvPosition.xyz;
    
    // Apply transformations to normals
    output.vNormal = normalize(mul(skinnedNormal, normalMatrix));
    
    // Calculate light incidence direction
    float4 lightPos = mul(float4(pointLightPosition, 1.0f), viewMatrix);
    output.lightDir = lightPos.xyz - mvPosition.xyz;
    
    // Calculate final position in clip space
    output.position = mul(mvPosition, projectionMatrix);
    
    return output;
}
