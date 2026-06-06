#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(push_constant) uniform PushConstants
{
    mat4 modelMatrix;
} push;

void main()
{
    // modelMatrix is the full clip-space transform (proj * view * local).
    gl_Position = push.modelMatrix * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}
