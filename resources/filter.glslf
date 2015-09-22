#version 120

varying vec2 texcoord;
uniform sampler2D frame1, frame2, mask;
uniform float gain;


// Reduce camera noise in the sensitive filter output with a median
// filter that chooses the middle color component and returns a
// grayscale value.

float medianFilter(vec3 v)
{
    if (v.r <= v.g) {
        if (v.b <= v.r) {
            return v.r;
        } else if (v.b <= v.g) {
            return v.b;
        } else {
            return v.g;
        }
    } else {
        if (v.b <= v.g) {
            return v.g;
        } else if (v.b <= v.r) {
            return v.b;
        } else {
            return v.r;
        }
    }
}

float diffSample(vec2 coord)
{
    vec3 color1 = texture2D(frame1, coord).rgb * gain;
    vec3 color2 = texture2D(frame2, coord).rgb * gain;
    return medianFilter(color1 - color2);
}

float boxFilter()
{
    float sum = 0.0f;

    const vec2 step = vec2(1.0/640.0, 1.0/480.0);
    const int kernel_size = 5;

    for (int x = -kernel_size; x <= kernel_size; x++) {
        for (int y = -kernel_size; y <= kernel_size; y++) {
            sum += diffSample(texcoord + vec2(x * step.x, y * step.y));
        }
    }

    return sum;
}

void main()
{
    gl_FragColor = vec4(vec3(boxFilter()), 1.0);
}