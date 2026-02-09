#version 120

varying vec2 vTexCoord;
varying float vAlong;
varying float vHalfWidth;

uniform vec3 u_DiffuseColor;
uniform sampler2D u_DiffuseTexture;
uniform bool u_HasDiffuseTexture;
uniform sampler2D u_NoiseTexture_1;
uniform float u_Time;
void main() {

    vec3 baseColor = u_DiffuseColor;
    float beamIntensity = 2.0;

    if (u_HasDiffuseTexture) {
        baseColor = texture2D(u_DiffuseTexture, vTexCoord).rgb;
        if (length(baseColor) < 0.01) {
            baseColor = u_DiffuseColor;
        }
    }


    float tWorld = (vTexCoord.y - 0.5) * vHalfWidth;
    float core = 1.0 - smoothstep(0.0, 0.2 * vHalfWidth, abs(tWorld));

    // Fade beam at start
    core *= smoothstep(0.0, 0.08, vAlong);


    vec2 ballPos = vec2(0.0, 0.5);
    float dx = vAlong - ballPos.x;
    float dy = vTexCoord.y - ballPos.y;
    float dist = sqrt(dx*dx + dy*dy);

    float ballRadius = 0.12;
    float edgeFalloff = ballRadius * 0.4;
    float ballMask = 1.0 - smoothstep(ballRadius - edgeFalloff, ballRadius + edgeFalloff, dist);

    vec3 ballColor = baseColor * 0.7;

    vec2 noiseUV1 = vTexCoord * 5.0 + vec2(u_Time * 1.0, u_Time * 0.5);
    vec2 noiseUV2 = vTexCoord * 10.0 + vec2(-u_Time * 7.0, u_Time * 0.3);

    float noise1 = texture2D(u_NoiseTexture_1, noiseUV1).r;
    float noise2 = texture2D(u_NoiseTexture_1, noiseUV2).r;
    float noise = noise1 + noise2;

    float noiseFactor = 0.4 + noise * 0.6;
    float beamGlow = core * beamIntensity * noiseFactor;
    float ballGlow = ballMask * 3.0;

    vec3 glowColor = baseColor * beamGlow + ballColor * ballGlow;

    float alpha = max(core, ballMask);

    gl_FragColor = vec4(glowColor, alpha);
}
