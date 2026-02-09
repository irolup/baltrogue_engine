struct FragmentInput {
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

uniform float3 u_CameraPos;
uniform float3 u_OutlineColor;
uniform float u_OutlinePower;

float4 main(FragmentInput input) : COLOR {
    float3 viewDir = normalize(u_CameraPos - input.worldPos);
    float3 normal  = normalize(input.normal);
    float NdotV = max(dot(normal, viewDir), 0.0f);
    float rim = 1.0f - NdotV;
    rim = pow(rim, u_OutlinePower);
    return float4(u_OutlineColor, rim);
}
