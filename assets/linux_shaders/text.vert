#version 120

attribute vec3 aPosition;
attribute vec2 aTexCoord;

uniform mat4 uViewProjectionMat;
uniform mat4 uModelMat;

varying vec2 texCoord;

void main()
{
    gl_Position = uViewProjectionMat * uModelMat * vec4(aPosition, 1.0);
    
    texCoord = aTexCoord;
}
