#version 310 es

precision highp float;
precision highp int;
//uniform sampler2D texture;
in vec2 v_textureCoords;
in float v_alpha;

out vec4 frag_color;

void main() {
    //vec3 tex_color = texture2D(texture, v_textureCoords).xyz;
    frag_color = vec4(1.,0.,0.,0.5);
}
