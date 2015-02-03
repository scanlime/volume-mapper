#version 120

varying vec2 texcoord;
uniform sampler2D filter, mask;
uniform float z_min, z_max, alpha;

float sampleFilter()
{
    float sum = 0.0f;

    const vec2 step = vec2(1.0/640.0, 1.0/480.0);
    const int kernel_size = 7;

    for (int x = -kernel_size; x < kernel_size; x++) {
        for (int y = -kernel_size; y < kernel_size; y++) {
            vec2 coord = texcoord + vec2(x * step.x, y * step.y);
            sum += texture2D(filter, coord).r;
        }
    }

    return sum;
}

void main()
{
    float z = texture2D(mask, texcoord).r;
    if (z > z_min && z <= z_max) {
        gl_FragColor = vec4(vec3(sampleFilter()), alpha);
    } else {
        discard;
    }
}
