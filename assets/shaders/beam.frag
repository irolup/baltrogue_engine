struct FragmentInput {
    float2 texCoord   : TEXCOORD0;
    float  vAlong     : TEXCOORD1;
    float  vHalfWidth : TEXCOORD2;
    float  vBeamLength : TEXCOORD3;
    float3 worldPos   : TEXCOORD4;
};

uniform float3 u_DiffuseColor;
uniform sampler2D u_DiffuseTexture;
uniform int u_HasDiffuseTexture;
uniform sampler2D u_NoiseTexture_1;
uniform float u_Time;

float4 main(FragmentInput input) : COLOR {
    float3 baseColor = u_DiffuseColor;
    float beamIntensity = 2.0f;

    if (u_HasDiffuseTexture) {
        float4 texSample = tex2D(u_DiffuseTexture, input.texCoord);
        baseColor = texSample.rgb;
        if (length(baseColor) < 0.01f)
            baseColor = u_DiffuseColor;
    }

    float tWorld = (input.texCoord.y - 0.5f) * input.vHalfWidth;
    float core = 1.0f - smoothstep(0.0f, 0.2f * input.vHalfWidth, abs(tWorld));
    core *= smoothstep(0.0f, 0.08f, input.vAlong);

    float2 ballPos = float2(0.0f, 0.5f);
    float dx = input.vAlong - ballPos.x;
    float dy = input.texCoord.y - ballPos.y;
    float dist = sqrt(dx*dx + dy*dy);

    float ballRadius = 0.12f;
    float edgeFalloff = ballRadius * 0.4f;
    float ballMask = 1.0f - smoothstep(ballRadius - edgeFalloff, ballRadius + edgeFalloff, dist);

    float3 ballColor = baseColor * 0.7f;

    float2 noiseUV1 = input.texCoord * 5.0f + float2(u_Time * 1.0f, u_Time * 0.5f);
    float2 noiseUV2 = input.texCoord * 10.0f + float2(-u_Time * 7.0f, u_Time * 0.3f);
    float noise1 = tex2D(u_NoiseTexture_1, noiseUV1).r;
    float noise2 = tex2D(u_NoiseTexture_1, noiseUV2).r;
    float noise = noise1 + noise2;

    float noiseFactor = 0.4f + noise * 0.6f;
    float beamGlow = core * beamIntensity * noiseFactor;
    float ballGlow = ballMask * 3.0f;

    float3 glowColor = baseColor * beamGlow + ballColor * ballGlow;
    float alpha = max(core, ballMask);

    return float4(glowColor, alpha);
}
