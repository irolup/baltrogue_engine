// CG Vertex Shader for PS Vita (VitaGL)
// Text rendering

struct VertexInput {
    float3 aPosition : POSITION;
    float4 aColor : COLOR;
    float2 aTexCoord : TEXCOORD0;
};

struct VertexOutput {
    float4 Position : POSITION;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
};

// Uniforms
uniform float4x4 uViewProjectionMat;
uniform float4x4 uModelMat;

VertexOutput main(VertexInput input) {
    VertexOutput output;
    
    // Calculate final position in clip space
    output.Position = mul(uViewProjectionMat, mul(uModelMat, float4(input.aPosition, 1.0)));
    
    // Pass through color and texture coordinates
    output.color = input.aColor;
    output.texCoord = input.aTexCoord;
    
    return output;
}