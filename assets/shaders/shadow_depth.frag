// CG Fragment Shader for PS Vita
// Packs the normalised light space depth into RGBA8.
//
// The Vita has no sampleable depth texture, so 24 bits of depth are spread over
// the four 8-bit colour channels. lighting.frag reverses this with unpackDepth.

struct FragmentInput {
    float4 clipPos : TEXCOORD0;
};

float4 main(FragmentInput input) : COLOR {
    // Same [0,1] window depth the desktop and Vulkan backends store.
    float depth = (input.clipPos.z / input.clipPos.w) * 0.5f + 0.5f;
    depth = saturate(depth);

    const float4 bitShift = float4(16777216.0f, 65536.0f, 256.0f, 1.0f);
    const float4 bitMask = float4(0.0f, 0.00390625f, 0.00390625f, 0.00390625f);

    float4 encoded = frac(depth * bitShift);
    encoded -= encoded.xxyz * bitMask;

    return encoded;
}
