#version 120

uniform sampler2D color, sprite;
varying vec2 texcoord;
varying float raw_z;
uniform float gain;

void main()
{
    if (raw_z <= 0.001) {
        discard;
    }

    gl_FragColor.rgb = texture2D(color, texcoord).rgb * gain;
    gl_FragColor.a = texture2D(sprite, gl_PointCoord).r;
}
