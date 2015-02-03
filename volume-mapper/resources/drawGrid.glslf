#version 120

varying vec2 texcoord;
uniform sampler2D layer;
uniform float gain;

void main()
{
    gl_FragColor = gain * texture2D(layer, texcoord);
}
