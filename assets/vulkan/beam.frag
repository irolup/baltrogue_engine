#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in float inAlong;
layout(location = 2) in float inHalfWidth;

layout(std140, set = 1, binding = 4) uniform ShaderMaterialUniforms {
    vec4 baseColor;
    vec4 textureFlags;
} uMaterial;

layout(set = 1, binding = 0) uniform sampler2D uDiffuseTexture;
layout(set = 1, binding = 1) uniform sampler2D uNoiseTexture_1;

layout(push_constant) uniform BeamPushConstants {
    vec4 beamStart;
    vec4 beamEnd;
    float beamHalfWidth;
    float time;
} uBeam;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 baseColor = uMaterial.baseColor.rgb;
    const float beamIntensity = 2.0;

    if (uMaterial.textureFlags.x > 0.5) {
        baseColor = texture(uDiffuseTexture, inTexCoord).rgb;
        if (length(baseColor) < 0.01) {
            baseColor = uMaterial.baseColor.rgb;
        }
    }

    float tWorld = (inTexCoord.y - 0.5) * inHalfWidth;
    float core = 1.0 - smoothstep(0.0, 0.2 * inHalfWidth, abs(tWorld));
    core *= smoothstep(0.0, 0.08, inAlong);

    vec2 ballPos = vec2(0.0, 0.5);
    float dx = inAlong - ballPos.x;
    float dy = inTexCoord.y - ballPos.y;
    float dist = sqrt(dx * dx + dy * dy);

    const float ballRadius = 0.12;
    const float edgeFalloff = ballRadius * 0.4;
    float ballMask = 1.0 - smoothstep(ballRadius - edgeFalloff, ballRadius + edgeFalloff, dist);

    vec3 ballColor = baseColor * 0.7;

    vec2 noiseUV1 = inTexCoord * 5.0 + vec2(uBeam.time * 1.0, uBeam.time * 0.5);
    vec2 noiseUV2 = inTexCoord * 10.0 + vec2(-uBeam.time * 7.0, uBeam.time * 0.3);

    float noise1 = texture(uNoiseTexture_1, noiseUV1).r;
    float noise2 = texture(uNoiseTexture_1, noiseUV2).r;
    float noise = noise1 + noise2;

    float noiseFactor = 0.4 + noise * 0.6;
    float beamGlow = core * beamIntensity * noiseFactor;
    float ballGlow = ballMask * 3.0;

    vec3 glowColor = baseColor * beamGlow + ballColor * ballGlow;
    float alpha = max(core, ballMask);

    outColor = vec4(glowColor, alpha);
}
