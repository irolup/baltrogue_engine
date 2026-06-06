// CG Fragment Shader for PS Vita (VitaGL)
// Text rendering

struct FragmentInput {
    float2 texCoord : TEXCOORD0;
};

// Uniforms
uniform sampler2D uFontAtlasTexture;
uniform float4 uColor;

float4 main(FragmentInput input) : COLOR {
    float alpha = tex2D(uFontAtlasTexture, input.texCoord).r;
    return float4(uColor.rgb, uColor.a * alpha);
}
