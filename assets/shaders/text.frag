// CG Fragment Shader for PS Vita (VitaGL)
// Text rendering

struct FragmentInput {
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
};

// Uniforms
uniform sampler2D uFontAtlasTexture;

float4 main(FragmentInput input) : COLOR {
    // Sample alpha from font atlas texture
    float alpha = tex2D(uFontAtlasTexture, input.texCoord).r;
    
    // Apply alpha to color
    return float4(input.color.rgb, input.color.a * alpha);
}