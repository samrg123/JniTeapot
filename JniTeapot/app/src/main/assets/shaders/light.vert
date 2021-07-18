#version 310 es

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aEmission;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 Emission;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    Emission = aEmission;
}
