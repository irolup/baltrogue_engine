// CG Vertex Shader for PS Vita (VitaGL)
// Text rendering

struct VertexInput {
    float3 aPosition : POSITION;
    float2 aTexCoord : TEXCOORD0;
};

struct VertexOutput {
    float4 Position : POSITION;
    float2 texCoord : TEXCOORD0;
};

// Uniforms
uniform float4x4 uViewProjectionMat;
uniform float4x4 uModelMat;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    output.Position = mul(uViewProjectionMat, mul(uModelMat, float4(input.aPosition, 1.0)));
    output.texCoord = input.aTexCoord;
    
    return output;
}
