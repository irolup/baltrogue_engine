struct VertexInput {
    float3 aPosition : POSITION;
};

struct VertexOutput {
    float4 position : POSITION;
    float3 TexCoords : TEXCOORD0;
};

uniform float4x4 view;
uniform float4x4 projection;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    output.TexCoords = input.aPosition;
    
    float4 pos = mul(float4(input.aPosition, 1.0), view);
    pos = mul(pos, projection);
    
    output.position = float4(pos.xy, pos.w, pos.w);
    
    return output;
}
