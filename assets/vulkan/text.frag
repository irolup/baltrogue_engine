#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uFontAtlasTexture;

layout(std140, set = 1, binding = 1) uniform TextMaterialUniforms
{
    vec4 color;
} uTextMaterial;

void main()
{
    float alpha = texture(uFontAtlasTexture, fragTexCoord).r;

    outColor = vec4( uTextMaterial.color.rgb, uTextMaterial.color.a * alpha );
}