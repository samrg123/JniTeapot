#version 310 es
#extension GL_EXT_shader_io_blocks : enable
precision highp float;
precision highp sampler2DShadow;

out vec4 FragColor;

uniform vec4 uColor;// w is reflectivity
uniform vec3 uLightDir;
uniform sampler2D uShadowMap;
uniform samplerCube uEnvMap;
uniform sampler2D uAlbedo;
uniform mat4 uLightView;

in VS_OUT {
    vec4 FragPos;
    vec4 FragPosLightSpace;
    vec3 Normal;
    vec2 UV;
} fs_in;

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

vec2 sample_disk(inout uint seed) {
    float r0 = rand(seed);
    float r1 = rand(seed);
    float r = sqrt(r0);
    float theta = 2. * PI * r1;
    return vec2(r * cos(theta), r * sin(theta));
}

bool in_frustum(vec4 clip) {
    return abs(clip.x) < clip.w && abs(clip.y) < clip.w && abs(clip.z) < clip.w;
}
float NEAR = 0.5;
float FAR = 7.5;
float FRUSTUMW = 1.0;
float calc_search_radius(float l_radius, float zreciever) {
    return l_radius * (zreciever - NEAR) / zreciever;
}

float calc_penumbra_radius(float l_radius, float zreciever, float zavg) {
    return (zreciever - zavg) / zavg * l_radius;
}

float project_light_uv(float size_uv, float zworld) {
    return size_uv * NEAR / zworld;
}

float zclip_to_eye(float z) {
    return FAR * NEAR / (FAR - z * (FAR - NEAR));
}

float pcf(in sampler2D shadow_map, in vec2 filter_radius, in vec2 uv, float current_depth, int num_samples, inout uint rng_state) {
    float accum = 0.;
    for (int i = 0; i < num_samples; ++i) {
        vec2 offset = sample_disk(rng_state) * filter_radius;
        float closest_depth = texture(shadow_map, uv + offset).r;
        accum += current_depth > closest_depth ? 0. : 1.;
    }
    return accum / float(num_samples);
}

float calc_blocker_dist(in sampler2D shadow_map, in vec2 search_radius, in vec2 uv, float zreciever, int num_samples, inout uint rng_state) {
    float dist = 0.;
    int num_blockers = 0;
    for (int i = 0; i < num_samples; ++i) {
        vec2 offset = sample_disk(rng_state) * search_radius;
        float closest_depth = texture(shadow_map, uv + offset).r;
        bool is_blocker = zreciever > closest_depth;
        if (is_blocker) {
            ++num_blockers;
            dist += closest_depth;
        }
    }
    if (num_blockers == 0) return -1.0;
    else if (num_blockers == num_samples) return 0.;
    else return dist / float(num_blockers);
}

// returns floating point number between 0.0 and 1.0, with 0.0 completely in shadow and 1.0 completely in light
float calc_visibility(in QuadLight l, in vec4 frag_pos, float cosine, float z_light, inout uint rng_state) {
    //    if(!in_frustum(frag_pos)) return 0.0;
    //    return 1.0;
    float world_light_radius = length(l.u) * 1.4 / 2.; // half of diagonal

    vec3 proj_coords = frag_pos.xyz / frag_pos.w;
    proj_coords = proj_coords * 0.5 + 0.5;
    vec2 uv = proj_coords.xy;

    float bias = max(0.005 * (1.0 - cosine), 0.005);
    float zreciever = proj_coords.z - bias;// in light clip space between [0,1]
    float light_zreciever = z_light;// in light view spcae between [0, INF]

    float closest_depth = texture(uShadowMap, uv).r;
    //    return pcf(uShadowMap, 5. / vec2(textureSize(uShadowMap, 0)), uv, zreciever, 32, rng_state);

    // STEP 1: blocker search
    vec2 search_radius = vec2(calc_search_radius(world_light_radius, light_zreciever) / FRUSTUMW);
    float zavg = calc_blocker_dist(uShadowMap, search_radius, uv, zreciever, 16, rng_state);
    // STEP 2: estimate penumbra
    if (zavg <= 0.) return abs(zavg);
    zavg = zclip_to_eye(zavg);
    float penumbra_radius = calc_penumbra_radius(world_light_radius, light_zreciever, zavg);
//        penumbra_radius *= 1. / FRUSTUMW;

    // STEP 3: do filtering
    vec2 filter_radius = vec2(project_light_uv(penumbra_radius, light_zreciever) / FRUSTUMW);
    //    vec2 filter_radius = 3. / vec2(textureSize(uShadowMap, 0));

    return pcf(uShadowMap, filter_radius, uv, zreciever, 16, rng_state);
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
        //        L += (albedo / PI) * light.emission * dot(irradiance, normal);
        L += (vec3(1) / PI) * light.emission * dot(irradiance, normal);
    }
    return L;
}

void main() {
    uint rng_state = init_random_seed(uint(gl_FragCoord.x), uint(gl_FragCoord.y));

    {
        quad_lights[0].pos = vec3(-0.25, 0.5, -0.25);
        quad_lights[0].u = vec3(0.0, 0.0, 0.5);
        quad_lights[0].v = vec3(0.5, 0.0, 0.0);
        //        quad_lights[0].emission = vec3(17., 12., 4.);
        quad_lights[0].emission = vec3(1., 1., 1.)*5.;
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

    float z_eye = -(uLightView * fs_in.FragPos).z;
    float cosine = max(dot(normal, light_dir), 0.);
    float vis = calc_visibility(quad_lights[0], fs_in.FragPosLightSpace, cosine, z_eye, rng_state);
    //    vec3 frag_color = cosine * albedo * vis;
    //    frag_color *= uColor.w == 0. ? vec3(1.) : texture(uEnvMap, normal).xyz * uColor.w;
    //    frag_color = vis * vec3(1,0,0);

//    vec3 frag_color = analytic_direct(fs_in.FragPos.xyz, normal, albedo) * vis;
    vec3 frag_color = vec3(1) * vis;

    FragColor = vec4(frag_color, 1.);
}
