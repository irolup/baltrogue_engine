#version 120

// Attributes (match VAO indices)
attribute vec3 position;  // 0
attribute vec3 normal;    // 1
attribute vec2 texCoord;  // 2
attribute vec3 tangent;   // 3

// Uniforms
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;

// Outputs to fragment shader
varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;
varying vec3 vViewPos;
varying vec3 vTangent;
varying vec3 vBitangent;

void main() {
    // World position
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldPos = worldPos.xyz;

    // View position
    vec4 viewPos = viewMatrix * worldPos;
    vViewPos = viewPos.xyz;

    // Transform normal to world space
    vNormal = normalize(normalMatrix * normal);

    // Transform tangent to world space
    vTangent = normalize(normalMatrix * tangent);

    // Compute bitangent
    vBitangent = normalize(cross(vNormal, vTangent));

    // Texture coordinate (flip Y to match Vita)
    vTexCoord = vec2(texCoord.x, 1.0 - texCoord.y);

    // Clip-space position
    gl_Position = projectionMatrix * viewPos;
}