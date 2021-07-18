#version 310 es
precision highp float;

in vec3 Emission;

out vec4 FragColor;

void main() {
    FragColor = vec4(Emission, 1.0);
}
