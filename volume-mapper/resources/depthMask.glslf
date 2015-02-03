#version 120

varying vec2 texcoord;
uniform sampler2D depth, background;

const float bias = 0.005;
const float max_spread = 0.002;

const vec2 step = vec2(1.0 / 640.0, 1.0 / 480.0);

bool sampleIsTooCloseToBackground(vec4 sample, vec2 coord)
{
    float bg = texture2D(background, coord).b;
    return bg > 0.0 && sample.b + bias > bg;
}

bool sampleIsTooFarFromNeighbor(vec4 sample, vec2 coord)
{
    return abs(sample.b - texture2D(depth, coord).b) >= max_spread;
}

bool sampleIsTooFarFromNeighbors(vec4 sample, vec2 coord)
{
    return
        sampleIsTooFarFromNeighbor(sample, coord + vec2(-step.x, -step.y)) ||
        sampleIsTooFarFromNeighbor(sample, coord + vec2(0.0, -step.y)) ||
        sampleIsTooFarFromNeighbor(sample, coord + vec2(step.x, -step.y)) ||
        sampleIsTooFarFromNeighbor(sample, coord + vec2(-step.x, 0.0)) ||
        sampleIsTooFarFromNeighbor(sample, coord + vec2(step.x, 0.0)) ||
        sampleIsTooFarFromNeighbor(sample, coord + vec2(-step.x, step.y)) ||
        sampleIsTooFarFromNeighbor(sample, coord + vec2(0.0, step.y)) ||
        sampleIsTooFarFromNeighbor(sample, coord + vec2(step.x, step.y));
}

void main()
{
    vec4 sample = texture2D(depth, texcoord);

    if (sampleIsTooCloseToBackground(sample, texcoord) ||
        sampleIsTooFarFromNeighbors(sample, texcoord)) {
        gl_FragColor = vec4(0.0);
    } else {
        gl_FragColor = sample;
    }
}
