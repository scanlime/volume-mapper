#version 120

uniform sampler2D depth;
uniform float point_size;
attribute vec2 position;
varying vec2 texcoord;
varying float raw_z;

void main()
{
    texcoord = position;

    raw_z = texture2D(depth, texcoord).r;

    gl_Position = gl_ModelViewProjectionMatrix * vec4(position.xy, raw_z, 1.0);
    gl_PointSize = point_size / gl_Position.w;
}
