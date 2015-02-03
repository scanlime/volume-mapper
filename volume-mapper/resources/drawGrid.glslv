#version 120

attribute vec2 position;
varying vec2 texcoord;
uniform float z;

void main() {
    texcoord = position;
    gl_Position = gl_ModelViewProjectionMatrix * vec4(position.xy, z, 1.0);
}
