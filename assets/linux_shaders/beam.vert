#version 120

attribute vec3 position;
attribute vec2 texCoord;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform vec3 u_BeamStart;
uniform vec3 u_BeamEnd;
uniform float u_BeamHalfWidth;
uniform vec3 u_CameraPos;

varying vec2 vTexCoord;
varying float vAlong;
varying float vHalfWidth;
varying float vBeamLength;
varying vec3 vWorldPos;

void main() {
    float s = position.x;
    float t = position.y;

    vAlong = 1.0 - s;
    vHalfWidth = u_BeamHalfWidth;
    vBeamLength = length(u_BeamEnd - u_BeamStart);

    vec3 along = mix(u_BeamStart, u_BeamEnd, s);

    vec3 beamDir = normalize(u_BeamEnd - u_BeamStart);
    vec3 viewDir = normalize(u_CameraPos - along);

    vec3 beamRight = normalize(cross(viewDir, beamDir));

    vec3 worldPos = along + beamRight * (t * u_BeamHalfWidth);

    vWorldPos = worldPos;

    gl_Position = projectionMatrix * viewMatrix * vec4(worldPos, 1.0);
    vTexCoord = vec2(texCoord.x, t * 0.5 + 0.5);
}
