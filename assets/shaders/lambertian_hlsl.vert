// HLSL Vertex Shader for PS Vita
// Alternative to CG shaders, compatible with Vita development tools

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOutput {
    float4 position : SV_POSITION;
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

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    // Calculate vertex position in modelview coordinate
    float4 mvPosition = mul(mul(float4(input.position, 1.0f), modelMatrix), viewMatrix);
    
    // View direction
    output.vViewPosition = -mvPosition.xyz;
    
    // Apply transformations to normals
    output.vNormal = normalize(mul(input.normal, normalMatrix));
    
    // Calculate light incidence direction
    float4 lightPos = mul(float4(pointLightPosition, 1.0f), viewMatrix);
    output.lightDir = lightPos.xyz - mvPosition.xyz;
    
    // Calculate final position in clip space
    output.position = mul(mvPosition, projectionMatrix);
    
    return output;
}
