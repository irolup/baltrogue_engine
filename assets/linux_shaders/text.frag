#version 120

varying vec2 texCoord;

uniform sampler2D uFontAtlasTexture;
uniform vec4 uColor;

void main()
{
    float alpha = texture2D(uFontAtlasTexture, texCoord).r;
    gl_FragColor = vec4(uColor.rgb, uColor.a * alpha);
}
