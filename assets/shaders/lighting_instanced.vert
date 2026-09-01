// CG Vertex Shader of instanced variant of lighting.vert

struct VertexInput {
    float3 aPosition : POSITION; // 0
    float3 aNormal : NORMAL; // 1
    float2 aTexCoord : TEXCOORD0; // 2
    float3 aTangent : TEXCOORD1; // 3

    float4 iModelCol0 : TEXCOORD2; // 4
    float4 iModelCol1 : TEXCOORD3; // 5
    float4 iModelCol2 : TEXCOORD4; // 6
    float4 iModelCol3 : TEXCOORD5; // 7
    float4 iNormalCol0 : TEXCOORD6; // 8
    float4 iNormalCol1 : TEXCOORD7; // 9
    float4 iNormalCol2 : TEXCOORD8; // 10
};

struct VertexOutput {
    float4 position : POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float3 viewPos : TEXCOORD3;
    float3 tangent : TEXCOORD4;
    float3 bitangent : TEXCOORD5;
};

uniform float4x4 viewMatrix;
uniform float4x4 projectionMatrix;

uniform float2 u_UVScale;
uniform float2 u_UVOffset;

VertexOutput main(VertexInput input) {
    VertexOutput output;

    // CG builds a matrix from rows, and InstanceData stores glm's columns, sothese come out transposed 
    float4x4 modelMatrix = float4x4(
        input.iModelCol0,
        input.iModelCol1,
        input.iModelCol2,
        input.iModelCol3
    );
    float3x3 normalMatrix = float3x3(
        input.iNormalCol0.xyz,
        input.iNormalCol1.xyz,
        input.iNormalCol2.xyz
    );

    float4 worldPos = mul(float4(input.aPosition, 1.0f), modelMatrix);
    output.worldPos = worldPos.xyz;

    float4 viewPos = mul(worldPos, viewMatrix);
    output.viewPos = viewPos.xyz;

    output.normal = normalize(mul(input.aNormal, normalMatrix));
    output.tangent = normalize(mul(input.aTangent, normalMatrix));
    output.bitangent = normalize(cross(output.normal, output.tangent));

    output.texCoord = input.aTexCoord * u_UVScale + u_UVOffset;

    output.position = mul(viewPos, projectionMatrix);

    return output;
}
