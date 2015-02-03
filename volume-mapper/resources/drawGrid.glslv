#version 120

attribute vec2 position;
varying vec2 texcoord;
uniform float z;

void main() {
    texcoord = position;
    gl_Position = gl_ModelViewProjectionMatrix * vec4(2.0 * position.x - 1.0, 2.0 * position.y - 1.0, z, 1.0);
}
