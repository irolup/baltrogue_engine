// CG Vertex Shader for PS Vita (VitaGL)
// Multi-light lighting system with explicit attribute locations

struct VertexInput {
    float3 aPosition : ATTR0;  // position
    float3 aNormal   : ATTR1;  // normal
    float2 aTexCoord : ATTR2;  // texture coordinates
    float3 aTangent  : ATTR3;  // tangent
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

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    // World position
    float4 worldPos = mul(float4(input.aPosition, 1.0f), modelMatrix);
    output.worldPos = worldPos.xyz;
    
    // View position
    float4 viewPos = mul(worldPos, viewMatrix);
    output.viewPos = viewPos.xyz;
    
    // Transform normal to world space
    output.normal = normalize(mul(input.aNormal, normalMatrix));
    
    // Transform tangent to world space
    output.tangent = normalize(mul(input.aTangent, normalMatrix));
    
    // Bitangent
    output.bitangent = normalize(cross(output.normal, output.tangent));
    
    // Texture coordinates (no flip needed for Vita)
    output.texCoord = input.aTexCoord;
    
    // Clip-space position
    output.position = mul(viewPos, projectionMatrix);
    
    return output;
}