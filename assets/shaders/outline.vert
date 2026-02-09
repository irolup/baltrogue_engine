struct VertexInput {
    float3 aPosition : POSITION;
    float3 aNormal   : NORMAL;
};

struct VertexOutput {
    float4 position : POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

uniform float4x4 modelMatrix;
uniform float4x4 viewMatrix;
uniform float4x4 projectionMatrix;
uniform float3x3 normalMatrix;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    float4 worldPos = mul(float4(input.aPosition, 1.0f), modelMatrix);
    output.worldPos = worldPos.xyz;
    output.normal   = normalize(mul(input.aNormal, normalMatrix));
    float4 viewPos  = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);
    return output;
}
