// CG Vertex Shader for PS Vita (VitaGL)
// Multi-light lighting system with explicit attribute locations

struct VertexInput {
    float3 aPosition : POSITION;
    float3 aNormal   : NORMAL;
    float2 aTexCoord : TEXCOORD0;
    float3 aTangent  : TEXCOORD1;
    float4 boneWeights : TEXCOORD2;
    float4 boneIndices : TEXCOORD3;
};

struct VertexOutput {
    float4 position  : POSITION;  // clip-space position
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 texCoord  : TEXCOORD2;
    float3 viewPos   : TEXCOORD3;
    float3 tangent   : TEXCOORD4;
    float3 bitangent : TEXCOORD5;
};

// Uniforms
uniform float4x4 modelMatrix;
uniform float4x4 viewMatrix;
uniform float4x4 projectionMatrix;
uniform float3x3 normalMatrix;

uniform float4x4 u_BoneMatrices[100];  // Maximum 100 bones
uniform int u_NumBones;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    float4 skinnedPosition = float4(input.aPosition, 1.0f);
    float3 skinnedNormal = input.aNormal;
    float3 skinnedTangent = input.aTangent;

    if (u_NumBones > 0) {
        int boneIndex0 = (int)input.boneIndices.x;
        int boneIndex1 = (int)input.boneIndices.y;
        int boneIndex2 = (int)input.boneIndices.z;
        int boneIndex3 = (int)input.boneIndices.w;
        
        // Clamp bone indices to valid range (manual clamping for CG compatibility)
        int maxBoneIndex = u_NumBones - 1;
        if (boneIndex0 < 0) boneIndex0 = 0;
        if (boneIndex0 > maxBoneIndex) boneIndex0 = maxBoneIndex;
        if (boneIndex1 < 0) boneIndex1 = 0;
        if (boneIndex1 > maxBoneIndex) boneIndex1 = maxBoneIndex;
        if (boneIndex2 < 0) boneIndex2 = 0;
        if (boneIndex2 > maxBoneIndex) boneIndex2 = maxBoneIndex;
        if (boneIndex3 < 0) boneIndex3 = 0;
        if (boneIndex3 > maxBoneIndex) boneIndex3 = maxBoneIndex;
        
        float4x4 boneTransform = 
            u_BoneMatrices[boneIndex0] * input.boneWeights.x +
            u_BoneMatrices[boneIndex1] * input.boneWeights.y +
            u_BoneMatrices[boneIndex2] * input.boneWeights.z +
            u_BoneMatrices[boneIndex3] * input.boneWeights.w;
        
        skinnedPosition = mul(float4(input.aPosition, 1.0f), boneTransform);
        
        float3x3 boneNormalMatrix = float3x3(
            boneTransform[0].xyz,
            boneTransform[1].xyz,
            boneTransform[2].xyz
        );
        skinnedNormal = normalize(mul(input.aNormal, boneNormalMatrix));
        
        skinnedTangent = normalize(mul(input.aTangent, boneNormalMatrix));
    }
    
    // World position
    float4 worldPos = mul(skinnedPosition, modelMatrix);
    output.worldPos = worldPos.xyz;
    
    // View position
    float4 viewPos = mul(worldPos, viewMatrix);
    output.viewPos = viewPos.xyz;
    
    // Transform normal to world space
    output.normal = normalize(mul(skinnedNormal, normalMatrix));
    
    // Transform tangent to world space
    output.tangent = normalize(mul(skinnedTangent, normalMatrix));
    
    // Bitangent
    output.bitangent = normalize(cross(output.normal, output.tangent));
    
    // Texture coordinates (no flip needed for Vita)
    output.texCoord = input.aTexCoord;
    
    // Clip-space position
    output.position = mul(viewPos, projectionMatrix);
    
    return output;
}