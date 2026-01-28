attribute vec3 aPosition;

uniform mat4 view;
uniform mat4 projection;

varying vec3 TexCoords;

void main() {
    TexCoords = aPosition;
    
    vec4 pos = projection * view * vec4(aPosition, 1.0);
    
    gl_Position = pos.xyww;
}
