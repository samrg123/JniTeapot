#version 310 es
precision highp float;

in vec2 tex_coord;
out vec4 FragColor;

uniform sampler2D shadow_map;

float near_plane = 0.5;
float far_plane = 5.0;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

void main() {
    float depth = texture(shadow_map, tex_coord).r;
    FragColor = vec4(vec3(LinearizeDepth(depth) / far_plane), 1.0); // perspective
//    FragColor = vec4(vec3(depth), 1.0); // orthograhic
}
