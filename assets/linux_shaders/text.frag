#version 120

varying vec4 color;
varying vec2 texCoord;

uniform sampler2D uFontAtlasTexture;

void main()
{
    float alpha = texture2D(uFontAtlasTexture, texCoord).r;
    gl_FragColor = vec4(color.rgb, color.a * alpha);
}
