#version 450

layout(quads) in;

layout(set=0, binding=0) uniform sampler2D heightMap;
layout(set=0, binding=1) uniform Camera {
    mat4 viewProj;
};

void main()
{
    vec2 uv = gl_TessCoord.xy;

    vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, uv.x);
    vec4 p2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, uv.x);
    vec3 p = mix(p1, p2, uv.y).xyz;

    float height = texture(heightMap, uv).r;
    p.y += height;

    gl_Position = viewProj * vec4(p, 1.0);
}
