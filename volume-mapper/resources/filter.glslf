#version 120

varying vec2 texcoord;
uniform sampler2D frame1, frame2;
uniform float gain;

void main()
{
    vec3 color1 = texture2D(frame1, texcoord).rgb * gain;
    vec3 color2 = texture2D(frame2, texcoord).rgb * gain;
    vec3 diff = color1 - color2;
    gl_FragColor = vec4(diff * diff, 1.0f);
}