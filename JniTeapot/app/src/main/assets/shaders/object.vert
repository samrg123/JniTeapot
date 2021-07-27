#version 310 es
#extension GL_EXT_shader_io_blocks : enable
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;

out VS_OUT {
    vec4 FragPos;
    vec4 FragPosLightSpace;
    vec3 Normal;
    vec2 UV;
} vs_out;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    vs_out.FragPos = model * vec4(aPos, 1.0);
    vs_out.FragPosLightSpace = lightSpace * vec4(vs_out.FragPos.xyz, 1.0);
    vs_out.Normal = transpose(inverse(mat3(model))) * aNormal;
    vs_out.UV = aUV;
}