#version 450
layout(location = 0) in vec3 outTexCoords;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform samplerCube uEnvironmentMap;

void main() {
    outColor = texture(uEnvironmentMap, outTexCoords);
}