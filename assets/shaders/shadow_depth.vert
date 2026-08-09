// CG Vertex Shader for PS Vita
// Depth-only pass that fills one shadow atlas tile.

struct VertexInput {
    float3 aPosition : POSITION;
    float3 aNormal   : NORMAL;
    float2 aTexCoord : TEXCOORD0;
    float3 aTangent  : TEXCOORD1;
    float4 boneWeights : TEXCOORD2;
    float4 boneIndices : TEXCOORD3;
};

struct VertexOutput {
    float4 position : POSITION;
    float4 clipPos  : TEXCOORD0;
};

uniform float4x4 u_LightViewProj;
uniform float4x4 modelMatrix;

uniform float4x4 u_BoneMatrices[100];
uniform int u_NumBones;

// Always 0. VitaGL binds Cg attributes by their order in the compiled program,
// so if the compiler dropped the attributes this shader does not read, the
// remaining ones would shift down and bone weights would be fed from the normal
// stream. Multiplying by a uniform the compiler cannot fold keeps all six
// attributes alive and the layout identical to lighting.vert.
uniform float u_AttributeGuard;

VertexOutput main(VertexInput input) {
    VertexOutput output;

    float4 skinnedPosition = float4(input.aPosition, 1.0f);

    if (u_NumBones > 0 && input.boneWeights.x > 0.0f) {
        int boneIndex0 = (int)input.boneIndices.x;
        int boneIndex1 = (int)input.boneIndices.y;
        int boneIndex2 = (int)input.boneIndices.z;
        int boneIndex3 = (int)input.boneIndices.w;

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
    }

    float4 worldPos = mul(skinnedPosition, modelMatrix);
    output.position = mul(worldPos, u_LightViewProj);

    // See u_AttributeGuard: keeps the unread attributes referenced. Zero at run
    // time, so it changes nothing.
    float guard = input.aNormal.x + input.aTexCoord.x + input.aTangent.x;
    output.position.z += guard * u_AttributeGuard;

    output.clipPos = output.position;

    return output;
}
