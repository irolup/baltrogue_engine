#version 120

/*
 * Lambertian illumination model
 * Formula: Ld = kd * Li (l * n)
 */

// Uniforms
uniform float Kd;
uniform vec3 diffuseColor;

// Varyings (inputs from vertex shader)
varying vec3 lightDir;
varying vec3 vNormal;
varying vec3 vViewPosition;

void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(lightDir);
    
    float NdotL = max(dot(l, n), 0.0);
    
    gl_FragColor = vec4(Kd * diffuseColor * NdotL, 1.0);
}
