#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoord;

layout(std140, set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    int numLights;
    int hasEnvironmentMap;
    int _pad1, _pad2;
} uFrame;

layout(push_constant) uniform BeamPushConstants {
    vec4 beamStart;
    vec4 beamEnd;
    float beamHalfWidth;
    float time;
} uBeam;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out float outAlong;
layout(location = 2) out float outHalfWidth;

void main() {
    float s = inPosition.x;
    float t = inPosition.y;

    outAlong = 1.0 - s;
    outHalfWidth = uBeam.beamHalfWidth;

    vec3 start = uBeam.beamStart.xyz;
    vec3 endPt = uBeam.beamEnd.xyz;
    vec3 along = mix(start, endPt, s);

    vec3 beamDir = normalize(endPt - start);
    vec3 viewDir = normalize(uFrame.cameraPosition.xyz - along);
    vec3 beamRight = normalize(cross(viewDir, beamDir));

    vec3 worldPos = along + beamRight * (t * uBeam.beamHalfWidth);

    gl_Position = uFrame.proj * uFrame.view * vec4(worldPos, 1.0);
    outTexCoord = vec2(inTexCoord.x, t * 0.5 + 0.5);
}
