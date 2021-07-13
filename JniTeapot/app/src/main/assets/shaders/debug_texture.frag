#version 310 es
precision highp float;

in vec2 tex_coord;
out vec4 FragColor;

uniform sampler2D shadow_map;

void main() {
//    float depth = texture(shadow_map, tex_coord).r;
//    FragColor = vec4(vec3(depth), 1.0);
    FragColor = vec4(1.,0.,0.,1.);
}
