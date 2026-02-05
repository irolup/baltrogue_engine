// CG Fragment Shader
// Custom Unlit Shader

struct FragmentInput {
    float2 texCoord : TEXCOORD0;
};

uniform float3 u_DiffuseColor;
uniform sampler2D u_DiffuseTexture;
uniform int u_HasDiffuseTexture;

float4 main(FragmentInput input) : COLOR {
    float3 color = u_DiffuseColor;
    
    if (u_HasDiffuseTexture) {
        float4 texSample = tex2D(u_DiffuseTexture, input.texCoord);
        color = texSample.rgb;
    }
    
    return float4(color, 1.0f);
}