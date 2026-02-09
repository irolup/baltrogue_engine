struct VertexInput {
    float3 aPosition : POSITION;
    float2 aTexCoord : TEXCOORD0;
};

struct VertexOutput {
    float4 position   : POSITION;
    float2 texCoord   : TEXCOORD0;
    float  vAlong     : TEXCOORD1;
    float  vHalfWidth : TEXCOORD2;
    float  vBeamLength : TEXCOORD3;
    float3 worldPos   : TEXCOORD4;
};

uniform float4x4 viewMatrix;
uniform float4x4 projectionMatrix;
uniform float3 u_BeamStart;
uniform float3 u_BeamEnd;
uniform float u_BeamHalfWidth;
uniform float3 u_CameraPos;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    float s = input.aPosition.x;
    float t = input.aPosition.y;

    output.vAlong = 1.0f - s;
    output.vHalfWidth = u_BeamHalfWidth;
    output.vBeamLength = length(u_BeamEnd - u_BeamStart);

    float3 along = lerp(u_BeamStart, u_BeamEnd, s);
    float3 beamDir = normalize(u_BeamEnd - u_BeamStart);
    float3 viewDir = normalize(u_CameraPos - along);
    float3 beamRight = normalize(cross(viewDir, beamDir));

    float3 worldPos = along + beamRight * (t * u_BeamHalfWidth);
    output.worldPos = worldPos;

    float4 worldPos4 = float4(worldPos, 1.0f);
    float4 viewPos = mul(worldPos4, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);
    output.texCoord = float2(input.aTexCoord.x, t * 0.5f + 0.5f);
    return output;
}
