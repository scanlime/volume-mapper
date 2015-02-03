#version 120

varying vec2 texcoord;
uniform sampler2D layer;
uniform float gain;

void main()
{
    gl_FragColor = gain * vec4(vec3(texture2D(layer, texcoord).r), 1.0);
}
