#version 120

attribute vec2 position;
varying vec2 texcoord;

void main() {
    texcoord = position;
    gl_Position = vec4(2.0 * position.x - 1.0, 2.0 * position.y - 1.0, 0.0, 1.0);
}
