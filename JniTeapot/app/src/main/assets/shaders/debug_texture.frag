#version 310 es
precision highp float;

in vec2 tex_coord;
out vec4 FragColor;

uniform sampler2D shadow_map;

void main() {
    float depth = texture(shadow_map, tex_coord).r;
    FragColor = vec4(vec3(1.-clamp(depth, 0, 1)), 1.0);
}
