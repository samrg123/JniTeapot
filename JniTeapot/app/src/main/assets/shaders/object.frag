#version 310 es
#extension GL_EXT_shader_io_blocks : enable
precision highp float;
precision highp sampler2DShadow;

out vec4 FragColor;

uniform vec4 color; // w is reflectivity
uniform vec3 lightDir;
uniform sampler2DShadow shadowMap;
uniform samplerCube envMap;
// automatic pcf with 4 closest pixels

in VS_OUT {
   vec3 FragPos;
   vec3 Normal;
   vec4 FragPosLightSpace;
} fs_in;

const vec2 PoissonDisk[16] = vec2[](
vec2( -0.94201624, -0.39906216 ),
vec2( 0.94558609, -0.76890725 ),
vec2( -0.094184101, -0.92938870 ),
vec2( 0.34495938, 0.29387760 ),
vec2( -0.91588581, 0.45771432 ),
vec2( -0.81544232, -0.87912464 ),
vec2( -0.38277543, 0.27676845 ),
vec2( 0.97484398, 0.75648379 ),
vec2( 0.44323325, -0.97511554 ),
vec2( 0.53742981, -0.47373420 ),
vec2( -0.26496911, -0.41893023 ),
vec2( 0.79197514, 0.19090188 ),
vec2( -0.24188840, 0.99706507 ),
vec2( -0.81409955, 0.91437590 ),
vec2( 0.19984126, 0.78641367 ),
vec2( 0.14383161, -0.14100790 )
);
const int NUM_SAMPLES = 4;
const float SPREAD = 800.0;

// returns floating point number between 0.0 and 1.0, with 0.0 completely in shadow and 1.0 completely in light
float calc_visibility(vec4 frag_pos, float cosine) {
   vec3 proj_coords = frag_pos.xyz / frag_pos.w;
   proj_coords = proj_coords * 0.5 + 0.5;

   float bias = max(0.005 * (1.0 - cosine), 0.005);
   float currentDepth = proj_coords.z - bias;

   float visibility = 0.0f;
   float sample_contrib = 1.0f / float(NUM_SAMPLES);
   for(int i = 0; i < NUM_SAMPLES; ++i) {
      visibility += sample_contrib * texture(shadowMap, vec3(proj_coords.xy + PoissonDisk[i]/SPREAD, currentDepth));
   }
   return visibility;
}

void main() {
   vec3 normal = normalize(fs_in.Normal);
   vec3 light_dir = normalize(lightDir);

   float cosine = max(dot(normal, light_dir), 0.);
   vec3 result = cosine * color.xyz * calc_visibility(fs_in.FragPosLightSpace, cosine);
   result *= color.w == 0. ? vec3(1.) : texture(envMap, normal).xyz * color.w;
   FragColor = vec4(result,1.);
}
