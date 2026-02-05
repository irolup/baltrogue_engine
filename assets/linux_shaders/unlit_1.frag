#version 120
// GLSL Fragment Shader
// Custom Unlit Shader

varying vec2 vTexCoord;

uniform vec3 u_DiffuseColor;
uniform sampler2D u_DiffuseTexture;
uniform bool u_HasDiffuseTexture;

void main() {
    vec3 color = u_DiffuseColor;
    
    if (u_HasDiffuseTexture) {
        color = texture2D(u_DiffuseTexture, vTexCoord).rgb;
    }
    
    gl_FragColor = vec4(color, 1.0);
}