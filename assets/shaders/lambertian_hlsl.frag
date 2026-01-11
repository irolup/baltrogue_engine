// HLSL Fragment Shader for PS Vita
// Alternative to CG shaders, compatible with Vita development tools

struct FragmentInput {
    float3 lightDir : TEXCOORD0;
    float3 vNormal : TEXCOORD1;
    float3 vViewPosition : TEXCOORD2;
};

// Uniforms
uniform float Kd;
uniform float3 diffuseColor;

float4 main(FragmentInput input) : SV_Target {
    float3 n = normalize(input.vNormal);
    float3 l = normalize(input.lightDir);
    
    float NdotL = max(dot(l, n), 0.0f);
    
    return float4(Kd * diffuseColor * NdotL, 1.0f);
}
