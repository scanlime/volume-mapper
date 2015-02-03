#version 120

varying vec2 texcoord;
uniform sampler2D filter, mask;
uniform float z_min, z_max, alpha;

void main()
{
    float z = texture2D(mask, texcoord).r;
    bool z_test = z > z_min && z <= z_max;
    if (!z_test) discard;
    gl_FragColor = vec4(vec3(texture2D(filter, texcoord).r), alpha);
}
