#version 120

// Attributes
attribute vec3 position;
attribute vec3 normal;

// Uniforms
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;
uniform vec3 pointLightPosition;

// Varyings (outputs to fragment shader)
varying vec3 lightDir;
varying vec3 vNormal;
varying vec3 vViewPosition;

void main() {
    // Calculating vertex position in modelview coordinate
    vec4 mvPosition = viewMatrix * modelMatrix * vec4(position, 1.0);
    
    // View direction
    vViewPosition = -mvPosition.xyz;
    
    // Applying transformations to normals
    vNormal = normalize(normalMatrix * normal);
    
    // Calculating light incidence direction
    vec4 lightPos = viewMatrix * vec4(pointLightPosition, 1.0);
    lightDir = lightPos.xyz - mvPosition.xyz;
    
    // Calculating final position in clip space
    gl_Position = projectionMatrix * mvPosition;
}
