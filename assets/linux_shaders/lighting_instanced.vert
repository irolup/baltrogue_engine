#version 120

// Instanced variant of lighting.vert

attribute vec3 position; // 0
attribute vec3 normal; // 1
attribute vec2 texCoord; // 2
attribute vec3 tangent; // 3

// see InstanceData in InstanceBatcher.h
attribute vec4 iModelCol0; // 4
attribute vec4 iModelCol1; // 5
attribute vec4 iModelCol2; // 6
attribute vec4 iModelCol3; // 7
attribute vec4 iNormalCol0; // 8
attribute vec4 iNormalCol1; // 9
attribute vec4 iNormalCol2; // 10

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform vec2 u_UVScale;
uniform vec2 u_UVOffset;

varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;
varying vec3 vViewPos;
varying vec3 vTangent;
varying vec3 vBitangent;

void main() {
    mat4 modelMatrix = mat4(iModelCol0, iModelCol1, iModelCol2, iModelCol3);
    mat3 normalMatrix = mat3(iNormalCol0.xyz, iNormalCol1.xyz, iNormalCol2.xyz);

    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldPos = worldPos.xyz;

    vec4 viewPos = viewMatrix * worldPos;
    vViewPos = viewPos.xyz;

    vNormal = normalize(normalMatrix * normal);
    vTangent = normalize(normalMatrix * tangent);
    vBitangent = normalize(cross(vNormal, vTangent));

    vec2 flippedTexCoord = vec2(texCoord.x, 1.0 - texCoord.y);
    vTexCoord = flippedTexCoord * u_UVScale + u_UVOffset;

    gl_Position = projectionMatrix * viewPos;
}
