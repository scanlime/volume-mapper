#version 120

varying vec2 texcoord;
uniform sampler2D filter, mask;
uniform float z_min, z_max, alpha;

void main()
{
    vec3 color = texture2D(filter, texcoord).rgb;
    float z = texture2D(mask, texcoord).b * 10.0;

    if (z > z_min && z <= z_max) {
        gl_FragColor = vec4(color, alpha);
    } else {
        discard;
    }
}
