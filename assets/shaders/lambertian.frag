// CG Fragment Shader for PS Vita (VitaGL)
// Lambertian illumination model
// Formula: Ld = kd * Li (l * n)

struct FragmentInput {
    float3 lightDir : TEXCOORD0;
    float3 vNormal : TEXCOORD1;
    float3 vViewPosition : TEXCOORD2;
};

// Uniforms
uniform float Kd;
uniform float3 diffuseColor;

float4 main(FragmentInput input) : COLOR {
    float3 n = normalize(input.vNormal);
    float3 l = normalize(input.lightDir);
    
    float NdotL = max(dot(l, n), 0.0f);
    
    return float4(Kd * diffuseColor * NdotL, 1.0f);
}
