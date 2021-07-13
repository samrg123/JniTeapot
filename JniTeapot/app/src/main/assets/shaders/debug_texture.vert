#version 310 es
precision highp float;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 tex_coord;

void main() {
    tex_coord = aTexCoords;
    gl_Position = vec4(vec3(0.0), 1.0);
}
