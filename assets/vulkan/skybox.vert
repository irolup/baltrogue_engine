#version 450
layout(location = 0) in vec3 inPosition;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 attenuation;
};

layout(std140, set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    int numLights;
    int hasEnvironmentMap;
    int _pad1, _pad2;
    Light lights[16];
} uFrame;


layout(location = 0) out vec3 outTexCoords;
void main() {
    outTexCoords = inPosition;
    mat3 viewRot = mat3(uFrame.view);
    vec4 pos = uFrame.proj * vec4(viewRot * inPosition, 1.0);
    gl_Position = vec4(pos.xy, pos.w, pos.w);
}