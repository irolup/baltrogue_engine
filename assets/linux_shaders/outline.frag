#version 120

uniform vec3 u_CameraPos;
uniform vec3 u_OutlineColor;
uniform float u_OutlinePower;

varying vec3 vWorldPos;
varying vec3 vNormal;

void main() {
    vec3 viewDir = normalize(u_CameraPos - vWorldPos);
    vec3 normal = normalize(vNormal);
    float NdotV = max(dot(normal, viewDir), 0.0);
    float rim = 1.0 - NdotV;
    rim = pow(rim, u_OutlinePower);
    gl_FragColor = vec4(u_OutlineColor, rim);
}
