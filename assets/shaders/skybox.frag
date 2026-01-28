struct FragmentInput {
    float3 TexCoords : TEXCOORD0;
};

uniform samplerCUBE skybox;

float4 main(FragmentInput input) : COLOR {
    return texCUBE(skybox, input.TexCoords);
}
