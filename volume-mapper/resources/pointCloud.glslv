#version 120

uniform sampler2D depth;
uniform float point_size;
attribute vec2 position;
varying vec2 texcoord;
varying float raw_z;

#define pi 3.141592653589793238462643383279

const float fov_x = 57.0 * (pi / 180.0);
const float fov_y = 43.0 * (pi / 180.0);

void main()
{
    texcoord = position;

    // Registered depth in millimeters
    raw_z = texture2D(depth, texcoord).b;

    float ray_x = fov_x * (0.5 - position.x);
    float ray_y = fov_y * (0.5 - position.y);
    float sx = sin(ray_x);
    float cx = cos(ray_x);
    float sy = sin(ray_y);
    float cy = cos(ray_y);

    float dist = raw_z * 5.0;
    vec3 p = dist * vec3(sx, sy, cx*cy);

    gl_Position = gl_ModelViewProjectionMatrix * vec4(p, 1.0);
    gl_PointSize = point_size / gl_Position.w;
}
