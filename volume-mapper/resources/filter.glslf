#version 120

varying vec2 texcoord;
uniform sampler2D frame1, frame2, mask;
uniform float gain;

void main()
{
    vec3 color1 = texture2D(frame1, texcoord).rgb * gain;
    vec3 color2 = texture2D(frame2, texcoord).rgb * gain;
    bool mask_value = texture2D(mask, texcoord).b > 0.0001;
    vec3 diff = color1 - color2;
    gl_FragColor = mask_value ? vec4(diff * diff, 1.0f) : vec4(0.0f);
}