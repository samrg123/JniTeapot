#version 310 es
#extension GL_EXT_shader_io_blocks : enable
precision highp float;
precision highp sampler2DShadow;

out vec4 FragColor;

uniform vec4 uColor;// w is reflectivity
uniform vec3 uLightDir;
uniform sampler2DShadow uShadowMap;
uniform samplerCube uEnvMap;
uniform sampler2D uAlbedo;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec4 FragPosLightSpace;
    vec2 UV;

} fs_in;

const vec2 PoissonDisk[16] = vec2[](
vec2(-0.94201624, -0.39906216),
vec2(0.94558609, -0.76890725),
vec2(-0.094184101, -0.92938870),
vec2(0.34495938, 0.29387760),
vec2(-0.91588581, 0.45771432),
vec2(-0.81544232, -0.87912464),
vec2(-0.38277543, 0.27676845),
vec2(0.97484398, 0.75648379),
vec2(0.44323325, -0.97511554),
vec2(0.53742981, -0.47373420),
vec2(-0.26496911, -0.41893023),
vec2(0.79197514, 0.19090188),
vec2(-0.24188840, 0.99706507),
vec2(-0.81409955, 0.91437590),
vec2(0.19984126, 0.78641367),
vec2(0.14383161, -0.14100790)
);
const int NUM_SAMPLES = 64;
const float SPREAD = 600.;

struct QuadLight {
    vec3 pos;
    vec3 u;
    vec3 v;
    vec3 emission;
};

#define NUM_QUAD_LIGHTS 1
#define PI 3.1415926535

QuadLight quad_lights[NUM_QUAD_LIGHTS];

// https://github.com/nvpro-samples/optix_prime_baking/blob/332a886f1ac46c0b3eea9e89a59593470c755a0e/random.h
// https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
uint init_random_seed(uint val0, uint val1){
    uint v0 = val0, v1 = val1, s0 = 0u;

    //[[unroll]]
    for (uint n = 0u; n < 16u; n++){
        s0 += 0x9e3779b9u;
        v0 += ((v1 << 4u) + 0xa341316cu) ^ (v1 + s0) ^ ((v1 >> 5u) + 0xc8013ea4u);
        v1 += ((v0 << 4u) + 0xad90777du) ^ (v0 + s0) ^ ((v0 >> 5u) + 0x7e95761eu);
    }

    return v0;
}

uint random_int(inout uint seed){
    // LCG values from Numerical Recipes
    return (seed = 1664525u * seed + 1013904223u);
}

float rand(inout uint seed){
    return (float(random_int(seed) & 0x00FFFFFFu) / float(0x01000000u));
}

// returns floating point number between 0.0 and 1.0, with 0.0 completely in shadow and 1.0 completely in light
float calc_visibility(in vec4 frag_pos, float cosine, inout uint rng_state) {
    vec3 proj_coords = frag_pos.xyz / frag_pos.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    float bias = max(0.005 * (1.0 - cosine), 0.005);
    float currentDepth = proj_coords.z - bias;

    float visibility = 0.0f;
    float sample_contrib = 1.0f / float(NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        vec2 jitter = vec2(rand(rng_state), rand(rng_state))/SPREAD;
        visibility += sample_contrib * texture(uShadowMap, vec3(proj_coords.xy+jitter, currentDepth));
    }
    return visibility;
}

vec3 analytic_direct(in vec3 r, in vec3 normal, in vec3 albedo) {
    vec3 L = vec3(0);
    for (int i = 0; i < NUM_QUAD_LIGHTS; ++i) {
        QuadLight light = quad_lights[i];

        vec3 v1 = light.pos;
        vec3 v2 = light.pos+light.u;
        vec3 v3 = light.pos+light.u+light.v;
        vec3 v4 = light.pos+light.v;

        vec3 irradiance = 0.5 * (acos(dot(normalize(v1 - r), normalize(v2 - r))) * normalize(cross(v1 - r, v2 - r))
        + acos(dot(normalize(v2 - r), normalize(v3 - r))) * normalize(cross(v2 - r, v3 - r))
        + acos(dot(normalize(v3 - r), normalize(v4 - r))) * normalize(cross(v3 - r, v4 - r))
        + acos(dot(normalize(v4 - r), normalize(v1 - r))) * normalize(cross(v4 - r, v1 - r)));
        L += (albedo / PI) * light.emission * dot(irradiance, normal);
    }
    return L;
}

void main() {
    uint rng_state = init_random_seed(uint(gl_FragCoord.x), uint(gl_FragCoord.y));

    {
        quad_lights[0].pos = vec3(-0.25, 0., -0.25);
        quad_lights[0].u = vec3(0.0, 0.0, 0.5);
        quad_lights[0].v = vec3(0.5, 0.0, 0.0);
        quad_lights[0].emission = vec3(1., 1., 1.);
        //
        //        quad_lights[0].pos = vec3(-0.7, 0.0, -0.7);
        //        quad_lights[0].u = vec3(0.0, 0.0, 1.4);
        //        quad_lights[0].v = vec3(0.0, 1.4, 0.0);
        //        quad_lights[0].emission = vec3(1., 0., 0.);
        //
        //        quad_lights[1].pos = vec3(0.25, 0.0, -0.25);
        //        quad_lights[1].v = vec3(0.0, 0.0, 0.5);
        //        quad_lights[1].u = vec3(0.0, 0.5, 0.0);
        //        quad_lights[1].emission = vec3(0., 1., 0.);
        //
        //        quad_lights[2].pos = vec3(-0.25, 0.5, -0.25);
        //        quad_lights[2].u = vec3(0.0, 0.0, 0.5);
        //        quad_lights[2].v = vec3(0.5, 0.0, 0.0);
        //        quad_lights[2].emission = vec3(1., 1., 1.);
    }

    vec3 normal = normalize(fs_in.Normal);
    vec3 light_dir = normalize(uLightDir);
    vec3 albedo = uColor == vec4(0.) ? texture(uAlbedo, fs_in.UV).xyz : uColor.xyz;

    // phong
    float cosine = max(dot(normal, light_dir), 0.);
    vec3 frag_color = cosine * albedo * calc_visibility(fs_in.FragPosLightSpace, cosine, rng_state);
    frag_color *= uColor.w == 0. ? vec3(1.) : texture(uEnvMap, normal).xyz * uColor.w;

    // no shadows yet :(
    //    vec3 frag_color = analytic_direct(fs_in.FragPos, normal, albedo);

    FragColor = vec4(frag_color, 1.);
}
