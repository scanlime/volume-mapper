#version 120

varying vec2 texcoord;
uniform sampler2D frame1, frame2, mask;
uniform float gain;

// Reduce camera noise in the sensitive filter output with a median
// filter that chooses the middle color component and returns a
// grayscale value.

vec3 medianFilter(vec3 v)
{
    if (v.r <= v.g) {
        if (v.b <= v.r) {
            return vec3(v.r);
        } else if (v.b <= v.g) {
            return vec3(v.b);
        } else {
            return vec3(v.g);
        }
    } else {
        if (v.b <= v.g) {
            return vec3(v.g);
        } else if (v.b <= v.r) {
            return vec3(v.b);
        } else {
            return vec3(v.r);
        }
    }
}

void main()
{
    vec3 color1 = texture2D(frame1, texcoord).rgb * gain;
    vec3 color2 = texture2D(frame2, texcoord).rgb * gain;
    bool mask_value = texture2D(mask, texcoord).r > 0.0001;
    vec3 diff = medianFilter(color1 - color2);
    gl_FragColor = mask_value ? vec4(diff * diff, 1.0f) : vec4(0.0f);
}