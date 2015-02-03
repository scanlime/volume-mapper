#version 120

varying vec2 texcoord;
uniform sampler2D depth, background;

const float bias = 0.005;
const float max_spread = 0.002;
const int erode = 16;
const vec2 step = vec2(1.0 / 640.0, 1.0 / 480.0);

void main()
{
    gl_FragColor = vec4(0.0);
    float threshold = texture2D(background, texcoord).b - bias;
    if (threshold <= 0.0) {
        return;
    }

    for (int x = -erode; x <= erode; x++) {
        for (int y = -erode; y <= erode; y++) {
            float sample = texture2D(depth, texcoord + vec2(step.x * x, step.y * y)).z;
            if (sample > threshold || sample < 1e-4) {
                return;
            }
        }
    }

    gl_FragColor = vec4(texture2D(depth, texcoord).r, 0.0, 0.0, 1.0);
}
