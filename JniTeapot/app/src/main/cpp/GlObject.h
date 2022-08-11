#pragma once

#include "GlRenderable.h"
#include "GlTransform.h"
#include "GlCamera.h"
#include "GlSkybox.h"

#include "util.h"
#include "FileManager.h"
#include "Memory.h"

class GlObject : public GlRenderable {
    private:
        
        enum Attribs      { ATTRIB_GEO_VERT, ATTRIB_NORMAL_VERT, ATTRIB_UV_VERT };
        enum TextureUnits { TU_SKY_MAP, TU_DEPTH_TEXTURE};
        enum Uniforms     { UNIFORM_MIRROR_CONSTANT, UNIFORM_LIGHT_POSITION, UNIFORM_CUBEMAP_MATRIX_INDEX };
        enum UBlocks      { UBLOCK_OBJECT };

        static inline constexpr int kUsePerspectiveDepthMap = 0;
        static inline constexpr int kNoDepthTest = 0;
        static inline constexpr int kDiffuseOnly = 0;
        static inline constexpr int kNoCubemapLight = 0;

        static inline constexpr StringLiteral kShaderVersion = "310 es";

        static inline constexpr StringLiteral kObjectBlock = Shader(
            ShaderUniformBlock(UBLOCK_OBJECT) ObjectBlock {
                mat4 mvpMatrix;
                mat4 modelMatrix;
                mat4 normalMatrix;
                mat4 cubemapMatrix[12];
                vec3 cameraPosition;
            };            
        );

        static inline constexpr StringLiteral kShaderFunctions = Shader(

            float interpolatedDepth(float linearDepth) {
                return 2.*smoothstep(-1., 1., linearDepth) - 1.;
            }

            vec4 depthProjection(vec3 position, mat4 projectionMatrix) {

                vec4 projectedPosition = projectionMatrix * vec4(position, 1.);
                
                vec2 scaledXY = projectedPosition.xy / abs(projectedPosition.w); 
                
                return vec4(scaledXY,
                            projectedPosition.z,
                            1.);
            }
        );

        static inline constexpr StringLiteral kVertexShaderSource = Shader(
            ShaderVersion(kShaderVersion)
            
            ShaderInclude(kObjectBlock)

            ShaderIn(ATTRIB_GEO_VERT)    vec3 position;
            ShaderIn(ATTRIB_NORMAL_VERT) vec3 normal;
            ShaderIn(ATTRIB_UV_VERT)     vec2 uv;

            ShaderOut(0) vec3 fragNormal;
            ShaderOut(1) vec3 fragWorldPosition;
            ShaderOut(2) vec3 fragPosition;

            void main() {

                vec4 v4Position = vec4(position, 1.);
                gl_Position = mvpMatrix*v4Position;

                fragNormal = mat3(normalMatrix) * normal;
                fragWorldPosition = (modelMatrix * v4Position).xyz;
                fragPosition = position;
            }
        );

        static inline constexpr StringLiteral kFragmentShaderSource = Shader(

            ShaderVersion(kShaderVersion)

            precision highp float;

            ShaderInclude(kObjectBlock)

            ShaderInclude(kShaderFunctions)
            ShaderInclude(ShaderConstants)

            ShaderSampler(TU_SKY_MAP)       samplerCube cubemapSampler;
            ShaderSampler(TU_DEPTH_TEXTURE) samplerCube depthTexture;

            ShaderUniform(UNIFORM_MIRROR_CONSTANT)  float mirrorConstant;
            ShaderUniform(UNIFORM_LIGHT_POSITION)   vec3 lightPosition;

            ShaderIn(0) vec3 fragNormal;
            ShaderIn(1) vec3 fragWorldPosition;
            ShaderIn(2) vec3 fragPosition;

            ShaderOut(0) vec4 fragColor;

            struct ObjectProperties {
                vec3 albedo;
                vec3 normal;
                float reflectivity;
                float diffuseness;
                float specularPower;
            };

            vec4 GetLightColor(vec3 textureRay, float lightLod) {
                
                const int kNoCubemapLight = ShaderValue(kNoCubemapLight);
                if(kNoCubemapLight != 0) {                
                    return vec4(1., 1., 1., 1.);
                }

                //Goofy sign change to flip right handed to openGL left handed coords
                vec3 lightTextureRay = vec3(-textureRay.x, textureRay.yz);              
                return textureLod(cubemapSampler, lightTextureRay, lightLod);
            }

            vec3 computeLight(ObjectProperties objectProperties, vec3 lightDirection, vec3 textureRay, float normalizedDepth) {

                const int kDiffuseOnly = ShaderValue(kDiffuseOnly);
                if(kDiffuseOnly != 0) {

                    // Get average light color from cubemap
                    vec3 lightColor = GetLightColor(textureRay, 1000.).rgb;
                    float inverseRSquared = 1.;

                    //Note: Lambertian law of cosine. assuming that each pixel acts as a point light source
                    vec3 radiance = lightColor * max(0., dot(objectProperties.normal, lightDirection));
                    vec3 diffuseIrradiance = radiance * inverseRSquared;
                    vec3 diffuseScaler = objectProperties.albedo / Pi();
                    vec3 diffuseRadiance = diffuseScaler * diffuseIrradiance;                    

                    return diffuseRadiance ;
                }

                //Average mipmap terms
                //TODO: pass in number of mimpas / use builtin GLSL function to compute min and max LOD
                float weights[10] = float[10] (
                    0., // 1., // 0., // 1. ,
                    0., // 2., // 0., // 1. ,
                    0., // 3., // 0., // 1. ,
                    1., // 4., // 0., // 1. ,
                    1., // 5., // 1., // 2. , //*This is the level where shadows start to become blobby and softer
                    2., // 6., // 0., // 3. ,
                    3., // 7., // 0., // 4. ,
                    4., // 8., // 0., // 5. ,
                    5., // 9., // 0., // 6. ,
                    6.  // 10. // 0.  // 7. 
                );
                
                float weightSum = 0.;
                for(int i = 0; i < weights.length(); ++i) {
                    weightSum+= weights[i];
                }
                float weightNormalizer = 1./weightSum;

                for(int i = 0; i < weights.length(); ++i) {
                    weights[i]*= weightNormalizer;
                }

                vec3 radientEnergy = vec3(0.); // in Joules 
                
                // vec4 texels = textureGather(depthTexture, lightDirection);
                // // float avgDepth = .25 * (texels.x + texels.y + texels.z + texels.w);
                
                // if(any(lessThanEqual(texels, vec4(normalizedDepth, normalizedDepth, normalizedDepth, normalizedDepth)))) {
                //     return vec3(1., 1., 1.);
                // }

                // for(int i = 0; i < 4; ++i) {
                //     float depthDelta = depth - texels[i];
                //     if(depthDelta >= 0.) {
                //         // radientEnergy.rgb+= weights[weightIndex] * incomingLight;
                //         radientEnergy.rgb+= .25 * vec3(1., 1., 1.);
                //     }
                // }
        
                // return radientEnergy;

                for(int weightIndex = 0; weightIndex < weights.length(); ++weightIndex) {

                    // float lod = -1000.;
                    float lod = float(weightIndex);
                    
                    // const float kLightLodOffset = 10.;
                    const float kLightLodOffset = 0.;
                    float lightLod = lod + kLightLodOffset;
                    // float lightLod = 10.;


                    //TODO: play with this and see what happens  
                    vec3 lightColor = GetLightColor(textureRay, lightLod).rgb;

                    float weight = weights[weightIndex];;

                    const int kNoDepthTest = ShaderValue(kNoDepthTest);
                    if(kNoDepthTest == 0) {

                        // Preform depth test

                        vec2 depthMapValue = textureLod(depthTexture, textureRay, lod).rg;
                        float depthMapDepth = depthMapValue.r;

                        float depthDelta = normalizedDepth - depthMapDepth;

                        //TODO: Pick a good value for this!
                        // const float kMinDepthDelta = 0.;
                        const float kMinDepthDelta = -.00005;
                        // const float kMinDepthDelta = -.00010;
                        // const float kMinDepthDelta = -.00025;
                        // const float kMinDepthDelta = -.0005;
                        // const float kMinDepthDelta = -.0010;
                        // const float kMinDepthDelta = -.010;
                        
                        
                        if(depthDelta < kMinDepthDelta) {

                            float m1 = depthMapDepth;
                            float m2 = depthMapValue.g; //g value is depth squared

                            float sigmaSquared = m2 - m1*m1;
                            float variance = depthDelta*depthDelta;

                            //Note: we only shade by the probability of being in shadow when variance is small. 
                            //      Otherwise we get light bleeding
                            //TODO: Look at the math and choose a good value for this
                            // const float kMaxVariance = .0005;
                            const float kMaxVariance = .001;
                            // const float kMaxVariance = .005;
                            // const float kMaxVariance = .010;
                            
                            if(variance > kMaxVariance) {

                                weight = 0.;
                                // return vec3(1., 0., 0.);

                            } else {

                                float pMax = sigmaSquared / (sigmaSquared + variance);
                                float pMaxClamp = clamp(pMax, 0., 1.);
                            
                                weight*= pMaxClamp;
                            }
                        }
                    }

                    float p = float(1<<weightIndex) / 1024.;
                    weight = p*weight + (1. - p)*weight*max(0., dot(objectProperties.normal, lightDirection));
  
                    // return vec3(depthDelta + 1., (depthDelta < kMinDepthDelta) ? 1. : 0., 0.);
                    // return vec3(1. - abs(depthMapDepth), (depthDelta < kMinDepthDelta) ? 1. : 0., 0.);

                    // weight = weights[weightIndex]; 

                    // const vec3 gammaDirections[] = vec3[6](
                    //     vec3( 0.,  0.,  1.),
                    //     vec3( 0.,  0., -1.),
                    //     vec3( 0.,  0.,  1.),
                    //     vec3( 0.,  0., -1.),
                    //     vec3(-1.,  0.,  0.),
                    //     vec3( 1.,  0.,  0.)
                    // );

                    // const vec3 gammaDirections2[] = vec3[6](
                    //     vec3( 0.,  1.,  0.),
                    //     vec3( 0.,  1.,  0.),
                    //     vec3( 1.,  0.,  0.),
                    //     vec3( 1.,  0.,  0.),
                    //     vec3( 0.,  1.,  0.),
                    //     vec3( 0.,  1.,  0.)
                    // );

                    // int gammaDirectionIndex;
                    // vec3 absLightDirection = abs(lightDirection);
                    // if(absLightDirection.x > absLightDirection.y) {
                        
                    //     gammaDirectionIndex = (absLightDirection.x > absLightDirection.z) ? (lightDirection.x >= 0. ? 0 : 1) : (lightDirection.z >= 0. ? 4 : 5);

                    // } else {
                    //     gammaDirectionIndex = (absLightDirection.y > absLightDirection.z) ? (lightDirection.y >= 0. ? 2 : 3) : (lightDirection.z >= 0. ? 4 : 5);
                    // }

                    // vec3 gamma  = float( 1<<weightIndex ) * gammaDirections[gammaDirectionIndex];

                    // float alpha = dot(objectProperties.normal, normalize( (1024./2.) * lightDirection + gamma));              
                    // float beta  = dot(objectProperties.normal, normalize( (1024./2.) * lightDirection - gamma));

                    // float upperBound = sqrt(1. - alpha*alpha);
                    // float lowerBound = sqrt(1. - beta*beta);

                    // vec3 gamma2  = float( 1<<weightIndex ) * gammaDirections2[gammaDirectionIndex];
                    // float alpha2 = dot(objectProperties.normal, normalize( (1024./2.) * lightDirection + gamma2));
                    // float beta2  = dot(objectProperties.normal, normalize( (1024./2.) * lightDirection - gamma2));
                    
                    // float upperBound2 = sqrt(1. - alpha2*alpha2);
                    // float lowerBound2 = sqrt(1. - beta2*beta2);

                    // if(gammaDirectionIndex == 5 | true) {
                    //     // weight*= max(0., upperBound - lowerBound);
                    //     // weight*= abs(max(0.,alpha) - max(0.,dot(objectProperties.normal, lightDirection)));
                    //     // weight*= sqrt(1. - alpha*alpha)*sqrt(1. - beta*beta);

                    //     // weight*= .5 * (min(1., upperBound) + min(1., lowerBound));
                    //     // weight*= .5 * (max(0., alpha) + max(0., beta));
                    //     // weight*= .5 * (max(0., alpha*alpha2) + max(0., beta*beta2));

                    //     // weight*= clamp(min(1., upperBound) - min(1., lowerBound), 0., 1.);
                    //     // weight*= clamp(upperBound - lowerBound, 0., 1.);

                    //     // float tmp1 = upperBound*upperBound2;
                    //     // float tmp2 = lowerBound*lowerBound2;
                    //     // weight*= clamp(.5*tmp1*(1. + sign(tmp1)) - .5*tmp2*(1. + sign(tmp2)) , 0., 1.);
                    
                    //     float tmp1 = .5*(alpha + alpha*alpha2) * (1./(sign(acos(alpha) * acos(alpha2))) + 1.);
                    //     float tmp2 = .5*(beta + beta*beta2) * (1./(sign(acos(beta) * acos(beta2))) + 1.);
                    //     weight*= clamp(tmp1 - tmp2, 0., 1.);
                        
                    //     // weight*= clamp(abs(min(1., upperBound) - min(1., lowerBound)), 0., 1.);

                    //     // lightColor.r = min(1., upperBound);
                    //     // lightColor.g = min(1., lowerBound);
                    //     // lightColor.b = clamp(dot(objectProperties.normal, lightDirection), 0., 1.);

                    //     // radientEnergy+= weight * lightColor;
                    //     // break;

                    // } else {
                    //     weight = 0.;
                    // }

                    radientEnergy+= weight * lightColor;
                }

                // return (dot(objectProperties.normal, lightDirection) <= 0.) ? vec3(0.) : (radientEnergy / 5.);

                // radientEnergy = vec3(1.);

                //TODO: also compute this value
                vec3 reflectedLight = vec3(0.);

                //TODO: Use average light for ambient?
                // const vec3 kAmbientLight = vec3(0.);
                // vec3 ambientTerm = objectProperties.albedo * ((objectProperties.diffuseness * kAmbientLight) + (objectProperties.reflectivity * reflectedLight));
                vec3 ambientRadiance = vec3(0.);
                                
                //Note: Integrated hemisphere radiance needs to sum to irradiance (total energy out = total energy in)
                //      lambertian diffuse surface has same radiance for each direction so albedo gets weighted by 1/pi
                vec3 diffuseScaler = objectProperties.albedo / Pi();
                
                //TODO: calculate this  
                float inverseRSquared = 1.;
 
                //Note: Lambertian law of cosine. assuming that each pixel acts as a point light source
                // vec3 diffuseIrradiance = radientEnergy * ( max(0., dot(objectProperties.normal, lightDirection)) * inverseRSquared );
                // vec3 diffuseIrradiance = radientEnergy * ( max(0., dot(objectProperties.normal, textureRay)) * inverseRSquared );
                vec3 diffuseIrradiance = radientEnergy * inverseRSquared;
                vec3 diffuseRadiance = diffuseScaler * diffuseIrradiance;

                //TODO: compuate this!
                // float specularTerm = objectProperties.reflectivity * pow(max(0., dot(cameraDirection, lightReflection)), specularPower);
                vec3 specularRadiance = vec3(0.);
                
                // vec3 lightRadiance = radientEnergy * (diffuseTerm + specularTerm) * invLightDistanceSquared;
                vec3 lightRadiance = diffuseRadiance + specularRadiance;
                
                return ambientRadiance + lightRadiance;
            }


            //TODO: this is phong - see if we should do blin-phong instead
            void main() {

                ObjectProperties objectProperties;

                // Note: Albedo of paper is between .6 and .7
                objectProperties.albedo = vec3(.65, .65, .65);
                // objectProperties.albedo = vec3(1., 1., 1.);

                //Note: interpolated fragNormal is not normalized
                objectProperties.normal = normalize(fragNormal);

                //objectProperties.reflectivity = mirrorConstant;
                objectProperties.reflectivity = 0.;

                // float diffuseness = 1. - mirrorConstant;
                objectProperties.diffuseness = 1. - 0.;

                objectProperties.specularPower = 8.; //Note: smaller numbers = more reflective

                // vec3 lightToVertex = lightPosition - fragWorldPosition;
                // float invLightDistanceSquared = 1./dot(lightToVertex, lightToVertex);

                // //Note: normal is not normalized!
                // vec3 normal = normalize(fragNormal);
                // vec3 lightDirection = normalize(lightToVertex);
                
                // vec3 vertexToCamera = cameraPosition - fragWorldPosition;
                // vec3 cameraDirection = normalize(vertexToCamera);

                // //TODO: play around with the direction vectors to cut down on the number of negations
                // vec3 lightReflection = -reflect(lightDirection, normal);
                // vec3 cubeReflection = -reflect(cameraDirection, normal);
                // cubeReflection.x = -cubeReflection.x;
                
                // //TODO: sit down and do the math to get an good roughness parameter
                // //float roughness = 20.; //0 is pure shiny
                // float roughness = 0.; //0 is pure shiny
                // float cubeLod = roughness * log( sqrt(length(vertexToCamera)) + 1.); //TODO: should use cubemapWall to vertex not vertexToCamera!
                
                // vec4 cubeColor = textureLod(cubemapSampler, cubeReflection, cubeLod);
    
                float lightRadius = 1.5;
                vec3 ambientColor = vec3(0., 0., 0.);

                vec3 cameraToVertex = fragWorldPosition - cameraPosition;
                vec3 textureRay = cameraToVertex;
                vec3 lightDirection = normalize(cameraToVertex);

                if(any(greaterThan(abs(textureRay), vec3(lightRadius, lightRadius, lightRadius)))) {

                    //Sanity Check                    
                    ambientColor.g = .5;

                } else {

                    vec3 ambientLight = vec3(0., 0., 0.);

                    const int kUsePerspective = ShaderValue(kUsePerspectiveDepthMap);
                    if(kUsePerspective == 1) {
                        
                        vec3 absTextureRay = abs(textureRay);
                        float depth;

                        if(absTextureRay.x >= max(absTextureRay.y, absTextureRay.z)) {

                            depth = absTextureRay.x / lightRadius;

                        } else if(absTextureRay.y >= max(absTextureRay.x, absTextureRay.z)) {

                            depth = absTextureRay.y / lightRadius;

                        } else {

                            depth = absTextureRay.z / lightRadius;

                        }
 
                        // if(absTextureRay.x >= max(absTextureRay.y, absTextureRay.z))
                        // if(absTextureRay.z >= max(absTextureRay.y, absTextureRay.x))
                        { 

                            //Note: test matrix has modelMatrix in it so we use fragPosition not fragWorldPosition!
                            vec4 projectedPosition;

                            vec3 testTextureRay = vec3(0.);
                            if(absTextureRay.x >= max(absTextureRay.y, absTextureRay.z)) {

                                projectedPosition = depthProjection(fragPosition, cubemapMatrix[ textureRay.x >= 0. ? 0 : 1 ]);

                                testTextureRay = (textureRay.x >= 0.) ? vec3( 1., -projectedPosition.y, -projectedPosition.x)
                                                                      : vec3(-1., -projectedPosition.y,  projectedPosition.x);

                            } else if(absTextureRay.y >= max(absTextureRay.x, absTextureRay.z)) {

                                projectedPosition = depthProjection(fragPosition, cubemapMatrix[ textureRay.y >= 2. ? 0 : 3 ]);

                                testTextureRay = (textureRay.y >= 0.) ? vec3( projectedPosition.x,  1.,  projectedPosition.y)
                                                                      : vec3( projectedPosition.x, -1., -projectedPosition.y);

                            } else {

                                projectedPosition = depthProjection(fragPosition, cubemapMatrix[ textureRay.z >= 0. ? 4 : 5 ]);

                                testTextureRay = (textureRay.z >= 0.) ? vec3( projectedPosition.x, -projectedPosition.y,  1.)
                                                                      : vec3(-projectedPosition.x, -projectedPosition.y, -1.);
                            }

                            vec3 testLightDirection = normalize(testTextureRay);

                            float testDepth = .5 * (projectedPosition.z + 1.);

                            // float depthRadius = length(vec3(projectedPosition.xy, testDepth));
                            // if(testDepth < 0.) depthRadius*= -1.;

                            // fragColor = vec4(computeLight(objectProperties, -testLightDirection, -testTextureRay, -depthRadius), 1.);
                            // return;

                            // testDepth = sign(testDepth) * length(vec3(projectedPosition.xy, testDepth));


                            // ambientLight+= computeLight(objectProperties,  testLightDirection,  testTextureRay,  testDepth);
                            ambientLight+= computeLight(objectProperties, -testLightDirection, -testTextureRay, -testDepth);
                        
                            vec3 oldLightVal = vec3(0.);
                            oldLightVal+= computeLight(objectProperties, -lightDirection, -textureRay, -depth);

                            vec3 lightDelta = ambientLight - oldLightVal;

                            float avgLightDelta = (lightDelta.x + lightDelta.y + lightDelta.z) / 3.;

                            // ambientLight = vec3(100.*abs(avgLightDelta), (avgLightDelta < 0.) ? 1. : 0. , 0.);
                            // ambientLight = 100000. * abs(testLightDirection - lightDirection);

                            bool oldMath = false; 
                            if(oldMath)
                            {
                                ambientLight = vec3(0.);

                                // Note: positive direction contributes very little light. Might be useful when viewing oblique angles 
                                // ambientLight+= computeLight(objectProperties, lightDirection,  textureRay,  depth);
                                
                                //Note: negative light direction contributes a majority of the light
                                ambientLight+= computeLight(objectProperties, -lightDirection, -textureRay, -depth);
                            }
                            
                            //TODO: -Figure out why we get upsidedown cow butt shadow when close to head
                            //      -Figure out if the depth term is correct
                            //      -Figure out what is causing the shadow cut-off bug when shadow spans faces and fix it!
                            //      -Right now this one ray only gives us light for 1 wall. Is this enough?
                            //      -Push what we have to the git repo
                            //      -pull out GlObjParser and update it so we can parse complex obj files like original umbrella
                            //      -See if we are supposed to alternate normal direction when we compute them in GlObjParser
                        }

                    } else {

                        vec3 depth = textureRay.xyz / lightRadius;

                        //add contribution from orthogonal lights
                        ambientLight+= computeLight(objectProperties, vec3( 1.,  0.,  0.), vec3( lightRadius,   textureRay.y,  textureRay.z),  depth.x);
                        ambientLight+= computeLight(objectProperties, vec3(-1.,  0.,  0.), vec3(-lightRadius,   textureRay.y,  textureRay.z), -depth.x);
                        ambientLight+= computeLight(objectProperties, vec3( 0.,  1.,  0.), vec3( textureRay.x,  lightRadius,   textureRay.z),  depth.y);
                        ambientLight+= computeLight(objectProperties, vec3( 0., -1.,  0.), vec3( textureRay.x, -lightRadius,   textureRay.z), -depth.y);
                        ambientLight+= computeLight(objectProperties, vec3( 0.,  0.,  1.), vec3( textureRay.x,  textureRay.y,  lightRadius),   depth.z);
                        ambientLight+= computeLight(objectProperties, vec3( 0.,  0., -1.), vec3( textureRay.x,  textureRay.y, -lightRadius),  -depth.z);

                        // ambientLight+= computeLight(objectProperties, -lightDirection, vec3( lightRadius,   textureRay.y,  textureRay.z),  depth.x);
                        // ambientLight+= computeLight(objectProperties, -lightDirection, vec3(-lightRadius,   textureRay.y,  textureRay.z), -depth.x);
                        // ambientLight+= computeLight(objectProperties, -lightDirection, vec3( textureRay.x,  lightRadius,   textureRay.z),  depth.y);
                        // ambientLight+= computeLight(objectProperties, -lightDirection, vec3( textureRay.x, -lightRadius,   textureRay.z), -depth.y);
                        // ambientLight+= computeLight(objectProperties, -lightDirection, vec3( textureRay.x,  textureRay.y,  lightRadius),   depth.z);
                        // ambientLight+= computeLight(objectProperties, -lightDirection, vec3( textureRay.x,  textureRay.y, -lightRadius),  -depth.z);
                    
                        // ambientLight*= 1./5.; //Note: 180 degree hemisphere can only collect light from at most 5 surfaces
                    }

                    ambientColor = ambientLight;
                }

                float gamma = 2.2;
                fragColor = vec4(pow(ambientColor.rgb, 1./vec3(gamma)), 1.);
                // fragColor = vec4(ambientColor.rgb, 1.);

                // vec3 ambientTerm = ambientColor.w * albedo.rgb * ((diffuseness*ambientColor.rgb) + (reflectivity*cubeColor.rgb));
                // vec3 diffuseTerm = diffuseness * albedo.rgb * max(0., dot(normal, lightDirection));

                // float specularTerm = reflectivity * pow(max(0., dot(cameraDirection, lightReflection)), specularPower);
                // vec3 lightTerm = fragLightColor.w * fragLightColor.rgb * (diffuseTerm+specularTerm) * invLightDistanceSquared;
                
                // fragColor.rgb = ambientTerm + lightTerm;
                // fragColor.a = albedo.a;
                
                // //uncomment to view NormalColor
                // fragColor.rgb = (.001*fragColor.rgb) + (.999*(.5*(objectProperties.normal + 1.)));
            }
        );

        static inline constexpr StringLiteral kVertexShaderRenderDepthTexture = Shader(
            ShaderVersion(kShaderVersion)

            precision highp float;

            ShaderInclude(kObjectBlock)

            ShaderInclude(kShaderFunctions)

            ShaderUniform(UNIFORM_CUBEMAP_MATRIX_INDEX) int cubemapMatrixIndex;

            ShaderIn(ATTRIB_GEO_VERT) vec3 position;

            ShaderOut(0) float fragLinearDepth;
            ShaderOut(1) vec2 fragXY;

            void main() {
                
                vec4 projectedPosition = depthProjection(position, cubemapMatrix[cubemapMatrixIndex]);
                gl_Position = projectedPosition;

                fragXY = projectedPosition.xy;
                fragLinearDepth = projectedPosition.z;
            }
        );
    
        static inline constexpr StringLiteral kFragmentShaderRenderDepthTexture = Shader(

            ShaderVersion(kShaderVersion)

            precision highp float;

            ShaderInclude(kObjectBlock)

            ShaderInclude(kShaderFunctions)
            
            ShaderIn(0) float fragLinearDepth;
            ShaderIn(1) vec2 fragXY;
    
            ShaderOut(0) vec2 fragColor;

            void main() {

                const int kUsePerspective = ShaderValue(kUsePerspectiveDepthMap);
                float depth = (kUsePerspective == 1) ? gl_FragCoord.z*2. - 1. : fragLinearDepth;

                // float depthRadius = length(vec3(fragXY, depth));
                // if(depth < 0.) depthRadius*= -1.;

                // fragColor.r = depthRadius;
                // fragColor.g = depthRadius*depthRadius;
                // return;

                fragColor.r = depth;
                fragColor.g = depth*depth;
            }
        );        
 
        enum Flag {
            FLAG_NORMAL                = 1<<0,
            FLAG_UV                    = 1<<1,
            FLAG_OBJ_TRANSFORM_UPDATED = 1<<2
        };

        struct alignas(16) UniformObjectBlock {
            Mat4<float> mvpMatrix;
            Mat4<float> modelMatrix;
            Mat4<float> normalMatrix;        
            Mat4<float> cubemapMatrix[12]; 
            
            //TODO: pad 4th dimension with something useful
            Vec3<float> cameraPosition;
        };
        
        GlSkybox* skybox;
        GlTransform transform;
        Mat4<float> transformMatrix;
        
        union {
            struct {
                GLuint vbo, elementBuffer, uniformObjectBlockBuffer;
            };
            GLuint glBuffers[3];
        };
        
        GLuint vao;
        GLuint glProgram, glProgramRenderDepthTexture;
        
        uint32 flags;
        uint32 numIndices;
        GLenum elementType;
        
        inline uint32 AllocateVBO(uint32 numVerts, uint32 vboStride) {
            uint32 vboBytes = numVerts*vboStride;
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            
            glBufferData(GL_ARRAY_BUFFER, vboBytes, nullptr, GL_STATIC_DRAW);
            GlAssertNoError("Failed to allocate vbo. { numVerts: %u, vboBytes: %u, vboStride: %u }",
                            numVerts, vboBytes, vboStride);
            
            return vboBytes;
        }

        inline uint32 AllocateElementsBuffer(uint32 numIndices, uint32 indexStride) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
            uint32 elementBufferBytes = numIndices*indexStride;

            glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementBufferBytes, nullptr, GL_STATIC_DRAW);
            GlAssertNoError("Failed to allocate element buffer { numIndices: %u, elementBufferBytes: %u, indexStride: %u }",
                            numIndices, elementBufferBytes, indexStride);
            
            return elementBufferBytes;
        }
        
        template<uint32 kAttribute, typename ArenaT>
        static inline uint32 InterleaveVboWithArena(const Memory::Arena* arena, uint32 numElements, void* vboPtr, uint32 vboStride, uint32 vboOffset) {
            
            glEnableVertexAttribArray(kAttribute);
            glVertexAttribPointer(kAttribute, GlAttributeSize<ArenaT>(),
                                  GlAttributeType<ArenaT>(), GL_FALSE, vboStride, reinterpret_cast<void*>(vboOffset));
    
            arena->CopyToBuffer<ArenaT>(numElements, ByteOffset(vboPtr, vboOffset), sizeof(ArenaT), vboStride);
            return vboOffset + sizeof(Vec3<float>);
        }
        
        struct UploadBufferParams {

            struct Indices {
                uint vertex;
                uint uv;
                uint normal;
            };        
            
            Memory::Arena geoVertArena, normalVertArena, uvVertArena;
            
            Memory::Arena* indicesArena;
            Memory::Region indicesStartRegion, indicesStopRegion;
            
            uint32 numIndices, numGeoVerts, numNormalVerts, numUvVerts;
        };
        
        template<typename ElementT>
        void UploadBuffers(UploadBufferParams* params) {
    
            numIndices = params->numIndices;
            uint32 numVerts = params->numGeoVerts;

            // Sanity Check that numbers make sense. 
            // Note: vertices can be used with multiple vertices. 
            //       `minIndices` is the logical lower bound on number of indices needed to represent all vertices
            uint32 minIndices = 3*numVerts;
            if(numIndices <= minIndices) {
                Warn("Num verts [%d] needs at least [%d] num indicies, but got [%d] num indicies? Ignoring extra vertices.",
                    numVerts, minIndices, numIndices);
            }

            elementType = GlAttributeType<ElementT>();
                        
            // compute vbo stride
            // Note: vbo always contain geoVerts & normals (we compute them if not provided). UV is optional
            uint32 vboStride = 2*sizeof(Vec3<float>);
            if(flags&FLAG_UV) vboStride+= sizeof(Vec2<float>);
            
            // allocate buffers
            glBindVertexArray(vao);
            uint32 vboBytes = AllocateVBO(numVerts, vboStride);
            uint32 elementBufferBytes = AllocateElementsBuffer(params->numIndices, sizeof(ElementT));
            
            void* vboPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, vboBytes, GL_MAP_WRITE_BIT);
            GlAssert(vboPtr, "Failed to map vbo buffer");
    
            void* elementPtr = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, elementBufferBytes, GL_MAP_WRITE_BIT) ;
            GlAssert(elementPtr, "Failed to map element buffer");
    
            //upload geoVerts
            uint32 vboOffset = InterleaveVboWithArena<ATTRIB_GEO_VERT, Vec3<float>>(&params->geoVertArena, numVerts, vboPtr, vboStride, 0);
            
            using Indicies = UploadBufferParams::Indices;

            // TODO: Now that we support 'shuffled' normal indicies cleanup this code and
            //       and allow for UV Indices as well

            if(flags&FLAG_NORMAL) {

                // Flatten normalVertArena into buffer
                uint32 numNormalVerts = params->numNormalVerts;
                uint32 normalBufferBytes = numNormalVerts * sizeof(Vec3<float>);
                Vec3<float>* normalBuffer = static_cast<Vec3<float>*>(params->normalVertArena.Flatten(normalBufferBytes));
                
                // Create tmpBuffer for vertex ordered buffer
                // TODO: we assign an average normal direction to vertex in the VBO. 
                //       This is technically wrong IE: cube can have two different normal directions per vertex
                //       what we really should do is have a separate normal buffer that is indexed indirectly in vertex shader via an index     
                Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
                Vec3<float>* tmpVertexOrderedNormalBuffer = static_cast<Vec3<float>*>(Memory::temporaryArena.PushBytes(numVerts * sizeof(Vec3<float>), true, alignof(Vec3<float>))); 

                //Translate indices to type T and copy to GL_ELEMENT_ARRAY_BUFFER
                Memory::TranslateRegionsToBuffer(
                    params->indicesStartRegion, params->indicesStopRegion,
                    params->numIndices, elementPtr,
                    sizeof(Indicies), sizeof(ElementT),
                    
                    [&](void* regionElement, void* bufferElement) {
                        
                        Indicies* indices = static_cast<Indicies*>(regionElement);

                        // add normal vector to vertex's average normal vector 
                        uint32 vertexIndex = indices->vertex;
                        tmpVertexOrderedNormalBuffer[indices->vertex]+= normalBuffer[indices->normal];

                        // Translate vertexIndex to ElementT type
                        *static_cast<ElementT*>(bufferElement) = ElementT(vertexIndex);
                    }
                );

                // Copy average normal direction to VBO
                for(uint32 i = 0; i < numVerts; ++i) {

                    Vec3<float>* vboNormal = static_cast<Vec3<float>*>(ByteOffset(vboPtr, vboOffset + vboStride*i));
                    *vboNormal = tmpVertexOrderedNormalBuffer[i].Normalize();
                }

                // Enable normal vertex 
                glEnableVertexAttribArray(ATTRIB_NORMAL_VERT);
                glVertexAttribPointer(
                    ATTRIB_NORMAL_VERT, GlAttributeSize<Vec3<float>>(),
                    GlAttributeType<Vec3<float>>(), GL_FALSE, 
                    vboStride, reinterpret_cast<void*>(vboOffset)
                );
                vboOffset+= sizeof(Vec3<float>);                

                // free tmpVertexOrderedNormalBuffer
                Memory::temporaryArena.FreeBaseRegion(tmpRegion);

            } else  {
                
                //TODO: clean this up and break out to function  
                
                Log("Normals not provided in obj file. Computing normals.");
                
                //create a temporary block of vectors filled with 0's to compute intermediate math on
                Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
                Vec3<float>* tmpNormals = (Vec3<float>*)Memory::temporaryArena.PushBytes(numVerts*sizeof(Vec3<float>), true);
                
                auto computeNormalsAndTranslateIndices = [&](void* regionElements, void* bufferElements) {
                    Indicies* indices = static_cast<Indicies*>(regionElements);
                    int index1 = indices[0].vertex,
                        index2 = indices[1].vertex,
                        index3 = indices[2].vertex;
    
                    //get corresponding vertices from vbo
                    Vec3<float> *v1 = (Vec3<float>*)ByteOffset(vboPtr, vboStride*index1),
                                *v2 = (Vec3<float>*)ByteOffset(vboPtr, vboStride*index2),
                                *v3 = (Vec3<float>*)ByteOffset(vboPtr, vboStride*index3);
    
                    //compute the add normal vector
                    Vec3<float> crossProduct = (*v2 - *v1).Cross(*v3 - *v1).Normalize();
                    tmpNormals[index1]+= crossProduct;
                    tmpNormals[index2]+= crossProduct;
                    tmpNormals[index3]+= crossProduct;
    
                    //translate indices to T
                    ElementT* translatedIndices = (ElementT*)bufferElements;
                    translatedIndices[0] = (ElementT)index1;
                    translatedIndices[1] = (ElementT)index2;
                    translatedIndices[2] = (ElementT)index3;
                };
                
                //compute smooth normals while translating indices to type T and copying to GL_ELEMENT_ARRAY_BUFFER
                Memory::TranslateRegionsToBuffer(params->indicesStartRegion, params->indicesStopRegion,
                                                 (params->numIndices)/3, elementPtr,
                                                 3*sizeof(Indicies), 3*sizeof(ElementT),
                                                 computeNormalsAndTranslateIndices);

                //average normals and interleave in vbo
                glEnableVertexAttribArray(ATTRIB_NORMAL_VERT);
                glVertexAttribPointer(ATTRIB_NORMAL_VERT, 3, GL_FLOAT, GL_FALSE, vboStride, reinterpret_cast<void*>(vboOffset));
                
                Memory::TranslateRegionsToBuffer(tmpRegion, Memory::temporaryArena.CreateRegion(),
                                                 numVerts, ByteOffset(vboPtr, vboOffset),
                                                 sizeof(Vec3<float>), vboStride,
                                                 [&](void* regionElement, void* bufferElement){ *(Vec3<float>*)bufferElement = ((Vec3<float>*)regionElement)->Normalize(); });
    
                Memory::temporaryArena.FreeBaseRegion(tmpRegion);
                vboOffset+= sizeof(Vec3<float>);
            }
            
            //upload uvVerts - TODO: TEST THIS WITH FILE
            if(flags&FLAG_UV) {

                uint32 numUvVerts = params->numUvVerts;
                RUNTIME_ASSERT(numUvVerts == 0, "UV not supported!");
                
                // NOTE: THIS CODE BELOW ONLY WORKS IF EACH uvIndex == geoIndex
                // vboOffset = InterleaveVboWithArena<ATTRIB_UV_VERT, Vec2<float>>(&params->uvVertArena, numVerts, vboPtr, vboStride, vboOffset);
            }
            
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            
            glBindVertexArray(0);
        }
        
        char* LoadVertex(char* strPtr, UploadBufferParams* params) {
            char mode = *++strPtr;
            switch(mode) {
        
                //geometry vertex
                case ' ':
                case '\t': {
                    ++params->numGeoVerts;
            
                    Vec3<float>* v = params->geoVertArena.PushType<Vec3<float>>();
                    v->x = StrToFloat(++strPtr, &strPtr);
                    v->y = StrToFloat(++strPtr, &strPtr);
                    v->z = StrToFloat(++strPtr, &strPtr);
            
                } break;
            
                //texture vertex
                case 't': {
                    ++params->numUvVerts;
            
                    Vec2<float>* v = params->uvVertArena.PushType<Vec2<float>>();
                    v->x = StrToFloat(++strPtr, &strPtr);
                    v->y = StrToFloat(++strPtr, &strPtr);
            
                } break;

                //normal vertex
                case 'n': {
                    ++params->numNormalVerts;
            
                    Vec3<float>* v = params->normalVertArena.PushType<Vec3<float>>();
                    v->x = StrToFloat(++strPtr, &strPtr);
                    v->y = StrToFloat(++strPtr, &strPtr);
                    v->z = StrToFloat(++strPtr, &strPtr);
            
                } break;
        
                default: {
                    Panic("Unsupported Vertex mode: %d[%c]", mode, mode);
                }
            }
            return strPtr;
        }
        
        char* LoadFace(char* strPtr, UploadBufferParams* params) {
            
            params->numIndices+= 3;
    
            auto assertValidVertexCount = [](int32 vertexCount) {
                RUNTIME_ASSERT(vertexCount >= 0, "Overflowed maximum allowed number of vertices [%d]", MaxInt32());
            };

            int32 numGeoVerts    = params->numGeoVerts;
            int32 numUvVerts     = params->numUvVerts;
            int32 numNormalVerts = params->numNormalVerts;

            assertValidVertexCount(numGeoVerts);
            assertValidVertexCount(numUvVerts);
            assertValidVertexCount(numNormalVerts);
            
            using Indices = UploadBufferParams::Indices;
            Indices* indiciesArray = static_cast<Indices*>(params->indicesArena->PushBytes(3*sizeof(Indices)));

            // TODO: Right now we only support triangles, but obj files can have arbitrary number of vertices in polygon
            //       at least throw a warning/error if we are truncating the polygon to just its first triangle 
            for(int i = 0; i < 3; ++i) {

                Indices& indicies = indiciesArray[i];

                // TODO: Pull out this code duplication into function

                //get vertIndex
                {
                    int geoIndex = StrToInt(++strPtr, &strPtr);
                    indicies.vertex = (geoIndex <= 0) ? geoIndex + numGeoVerts : geoIndex - 1;
                    RUNTIME_ASSERT(indicies.vertex < numGeoVerts, "Geometry vertex not defined { geoIndex: %d, numGeoVerts: %d }", geoIndex, numGeoVerts);
                }
 
                if(*strPtr != '/') continue;

                //get uvIndex
                {
                    flags|= FLAG_UV;
            
                    int uvIndex = StrToInt(++strPtr, &strPtr);
                    indicies.uv = (uvIndex <= 0) ? uvIndex + numUvVerts : uvIndex - 1; 
                    RUNTIME_ASSERT(indicies.uv == 0 || indicies.uv < numUvVerts, "UV vertex not defined { uvIndex: %d, numUvVerts: %d }", uvIndex, numUvVerts);
                }
        
                if(*strPtr != '/') continue; 
 
                //get normalIndex
                {
                    flags|= FLAG_NORMAL;
            
                    int normalIndex = StrToInt(++strPtr, &strPtr);
                    indicies.normal = (normalIndex <= 0) ? normalIndex + numNormalVerts : normalIndex - 1; 
                    RUNTIME_ASSERT(indicies.normal < numNormalVerts, "Normal vertex not defined { normalIndex: %d, numNormalVerts: %d }", normalIndex, numNormalVerts);
                }
            }
            
            return strPtr;
        }
        
        void LoadObject(const FileManager::AssetBuffer* buffer) {
    
            UploadBufferParams uploadParams = {
                .indicesArena = &Memory::temporaryArena,
                .indicesStartRegion = Memory::temporaryArena.CreateRegion()
            };
        
            char* ptr = (char*)buffer->data;
            for(;;) {
                ptr = SkipWhiteSpace(ptr);
                
                char c = *ptr;
                switch(c) {

                    //eof - Finish processing and return
                    case 0: {
                        
                        uploadParams.indicesStopRegion = Memory::temporaryArena.CreateRegion();
                        
                             if(!LargerThan8Bit(uploadParams.numGeoVerts))  UploadBuffers<uint8>(&uploadParams);
                        else if(!LargerThan16Bit(uploadParams.numGeoVerts)) UploadBuffers<uint16>(&uploadParams);
                        else UploadBuffers<uint32>(&uploadParams);
    
                        uploadParams.indicesArena->FreeBaseRegion(uploadParams.indicesStartRegion);
                        return;
                    }
                    
                    //ignore comments
                    case '#': break;
    
                    //handle vertices
                    case 'v': ptr = LoadVertex(ptr, &uploadParams);
                    break;
        
                    
                    //Note: faces - defined by indices in the form 'v/vt/vn' where vt and vn are optional
                    case 'f': ptr = LoadFace(ptr, &uploadParams);
                    break;

                    // TODO: this is just to stop annoying calls to panic for common obj tags
                    //       we really should be checking the full word, not just the start character 
                    case 'o': Warn("Ignoring objected tag 'o'?");
                    break;

                    case 's': Warn("Ignoring smooth shading tag 's'?");
                    break;

                    case 'm': Warn("Ignoring material tag 'mtllib'?");
                    break;

                    case 'u': Warn("Ignoring use material tag 'usemtl'?");
                    break;
                    
                    default: {
                        Panic("Unknown object command: %d[%c]", c, c);
                    }
                }

                //advance to next line
                ptr = SkipLine(ptr);
            }
        }

    public:
        
        GlObject(const char* objPath, GlCamera* camera, GlSkybox* skybox, const GlTransform& transform = GlTransform()):
                    GlRenderable(camera),
                    skybox(skybox),
                    transform(transform),
                    flags(FLAG_OBJ_TRANSFORM_UPDATED) {
            
            SetCamera(camera);
            
            RUNTIME_ASSERT(skybox, "Skybox is nullptr");
            
            glGenVertexArrays(1, &vao);
            GlAssertNoError("Failed to create vao");
            
            glGenBuffers(ArrayCount(glBuffers), glBuffers);
            GlAssertNoError("Failed to create glBuffers");
        
            glProgram = GlContext::CreateGlProgram(kVertexShaderSource, kFragmentShaderSource);
            glProgramRenderDepthTexture = GlContext::CreateGlProgram(kVertexShaderRenderDepthTexture, kFragmentShaderRenderDepthTexture);
            
            //Debugging
            GlContext::PrintVariables(glProgram);
            
            //bind uniform block to program
            glBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_OBJECT, uniformObjectBlockBuffer);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformObjectBlock), nullptr, GL_DYNAMIC_DRAW);
            GlAssertNoError("Failed to bind UniformBlock [%d] to uniformBlockBuffer [%d]", UBLOCK_OBJECT, uniformObjectBlockBuffer);
            
            // load obj
            Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
            FileManager::AssetBuffer* buffer = FileManager::OpenAsset(objPath, &Memory::temporaryArena);
            LoadObject(buffer);
            
            Memory::temporaryArena.FreeBaseRegion(tmpRegion);
            
        }
        
        ~GlObject() {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(ArrayCount(glBuffers), glBuffers);
            glDeleteProgram(glProgram);
            glDeleteProgram(glProgramRenderDepthTexture);
        }
        
        inline GlTransform GetTransform() const { return transform; }
        inline void SetTransform(const GlTransform& t) { transform = t; flags|= FLAG_OBJ_TRANSFORM_UPDATED; }
        

        //TODO: make this private
        void UpdateUniformBlock() {

            //check if the object matrix updated
            bool updateUniformObjectBlock;
            if(flags&FLAG_OBJ_TRANSFORM_UPDATED) {
                updateUniformObjectBlock = true;
                transformMatrix = transform.Matrix();
            } else {
                //check if camera updated
                updateUniformObjectBlock = CameraUpdated();
            }
        
            //update mvpMatrix
            if(updateUniformObjectBlock) {
                ApplyCameraUpdate();
                
                glBindBuffer(GL_UNIFORM_BUFFER, uniformObjectBlockBuffer);
                UniformObjectBlock* uniformObjectBlock = (UniformObjectBlock*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(UniformObjectBlock), GL_MAP_WRITE_BIT);
                GlAssert(uniformObjectBlock, "Failed to map uniformObjectBlock");
      
                //upload mvpMatrix
                uniformObjectBlock->mvpMatrix = camera->Matrix() * transformMatrix;
                
                //upload mvMatrix
                if(flags&FLAG_OBJ_TRANSFORM_UPDATED) {
                    flags^= FLAG_OBJ_TRANSFORM_UPDATED;

                    uniformObjectBlock->modelMatrix = transformMatrix;
                    uniformObjectBlock->normalMatrix = transform.NormalMatrix();
                }

                //upload camera position
                GlTransform cameraTransform = camera->GetTransform();
                uniformObjectBlock->cameraPosition = cameraTransform.position;                

                //Update cubemap projetion matrices
                {
                    //TODO: add constexpr support to directions and Mat4!
                    //TODO: Use camera's draw distance for projection matrix near/far planes!
                    float lightRadius = 1.5f;

                    
                    Mat4<float> cubemapProjectionMatrix;
                    Mat4<float> negCubemapProjectionMatrix;
                    if constexpr(kUsePerspectiveDepthMap) {

                        //Note: we want a smooth mapping of depth from -lightRadius to lightRadius. 
                        //      If we used normal PerspectiveProjection OpenGl implicit divide by w would map z to an reciprocal function that is which is undefined at 0. 
                        //      Instead we define a 90 degree FOV perspective matrix and, manually divide x,y by abs(w) in vertex shader while perseving linear mapping of z
                        //Note: We need to divide by abs(w) because w flips sign when z flips signs and w only acts as a perspective scaling factor based on the distance to camera
                        cubemapProjectionMatrix = Mat4<float>({{
                            1,  0,   0,              0,
                            0,  1,   0,              0,
                            0,  0,  -2/lightRadius, -1,
                            0,  0,  -1,              0,
                        }});

                        negCubemapProjectionMatrix = Mat4<float>({{
                           -1,  0,   0,              0,
                            0, -1,   0,              0,
                            0,  0,  -2/lightRadius, -1,
                            0,  0,   1,              0,
                        }});                        

                    } else {

                        cubemapProjectionMatrix = Mat4<float>::OrthogonalProjection(Cuboid<float> {
                            -lightRadius, lightRadius,
                            -lightRadius, lightRadius,
                            -lightRadius, lightRadius
                        });

                    }

                    auto qReflectYAxis = Quaternion(Vec3<float>::right, ToRadians(180.f));
                    Quaternion<float> rotations[] = {  
                        Quaternion<float>::RotateTo(Vec3<float>::in, Vec3<float>::right) * qReflectYAxis,
                        Quaternion<float>::RotateTo(Vec3<float>::in, Vec3<float>::left)  * qReflectYAxis,
                        Quaternion<float>::RotateTo(Vec3<float>::in, Vec3<float>::up)    * qReflectYAxis,
                        Quaternion<float>::RotateTo(Vec3<float>::in, Vec3<float>::down)  * qReflectYAxis,
                        Quaternion<float>::RotateTo(Vec3<float>::in, Vec3<float>::in)    * qReflectYAxis,
                        Quaternion<float>::RotateTo(Vec3<float>::in, Vec3<float>::out)   * qReflectYAxis,
                    };

                    constexpr int kNumCubemapMatrices = ArrayCount(UniformObjectBlock{}.cubemapMatrix);
                    static_assert(2*ArrayCount(rotations) == kNumCubemapMatrices);

                    GlTransform cubemapCameraTransform = cameraTransform;
                    for(int i = 0; i < ArrayCount(rotations); ++i) {

                        //rotate cubemap camera to look in cubemap face direction
                        cubemapCameraTransform.SetRotation(rotations[i]);

                        //compute cubemap matrix
                        Mat4<float> cubemapViewMatrix = cubemapCameraTransform.InverseMatrix();
                        Mat4<float> modelViewMatrix = cubemapViewMatrix * transformMatrix;
                        
                        uniformObjectBlock->cubemapMatrix[i] = cubemapProjectionMatrix * modelViewMatrix;
                        uniformObjectBlock->cubemapMatrix[i+6] = negCubemapProjectionMatrix * modelViewMatrix;
                    }
                }

                glUnmapBuffer(GL_UNIFORM_BUFFER);
            }            
        }


        void RenderToDepthTexture(const GlContext* context) {
            
            skybox->AttachFBO();

            glDisable(GL_BLEND);
            glDepthFunc(GL_GEQUAL);
            glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            
            glUseProgram(glProgramRenderDepthTexture);

            UpdateUniformBlock();

            //Note: no need to bind 'GL_ELEMENT_ARRAY_BUFFER', its part of vao state
            glBindVertexArray(vao);
            GlAssertNoError("Failed to bind vao");

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            GlAssertNoError("Failed to bind vbo");

            // TODO: Looks like glBindBufferBase doesn't save binding to glProgram? Is there a way to make this binding persist
            glBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_OBJECT, uniformObjectBlockBuffer);
            GlAssertNoError("Failed to bind uniformObjectBlockBuffer");

            const GLuint drawBuffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &drawBuffer);

            // GLuint cubemapDepthTexture = skybox->CubeMapDepthTexture();

            //Note: we draw 2 instances per cubemap face. One with the camera looking at the cubemap face and one looking away
            constexpr int kNumCubemapMatrices = ArrayCount(UniformObjectBlock{}.cubemapMatrix);
            for(int i = 0; i < kNumCubemapMatrices/2; ++i) {
        
                skybox->BindDepthTexture(i);

                // //Note: we draw 2 instances per cubemap face. One with the camera looking at the cubemap face and one looking away
                // int textureIndex = i;
                // glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                //                        drawBuffer,
                //                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + textureIndex,
                //                        cubemapDepthTexture, 0
                //                       );

                // glFramebufferTexture2D(
                //     GL_DEPTH_ATTACHMENT,
                //     depthBuffer,
                //     GL_TEXTURE_CUBE_MAP_POSITIVE_X + textureIndex,
                //     cubemapDepthTexture, 0
                // );

                // //TODO: Right now depth buffer is shared accross all cubemap faces so we need to clear between rendering 
                // //      Really should create a separate depth buffer for each face so rendering multiple objects work
                // //      When thats done move this out to GlSkybox::ClearDepthTexture()
                // glClearDepthf(0.f); 
                // glClear(GL_DEPTH_BUFFER_BIT);
                // glClearDepthf(1.f);

                if constexpr(kUsePerspectiveDepthMap) {

                    //draw ray moving towards cubemap face
                    glDepthRangef(.5f, 1.f);
                    glUniform1i(UNIFORM_CUBEMAP_MATRIX_INDEX, i);
                    glDrawElements(GL_TRIANGLES, numIndices, elementType, nullptr);

                    //draw ray moving away from cubemap face
                    glDepthRangef(0.f, .5f);
                    glUniform1i(UNIFORM_CUBEMAP_MATRIX_INDEX, i+6);
                    glDrawElements(GL_TRIANGLES, numIndices, elementType, nullptr);

                } else {

                    glUniform1i(UNIFORM_CUBEMAP_MATRIX_INDEX, i);
                    glDrawElements(GL_TRIANGLES, numIndices, elementType, nullptr);

                } 

                GlAssertNoError("Failed to Render Depth Texture - Face: %d", i);
            }
            
            glDepthRangef(.0f, 1.f);

            skybox->DetachFBO(context);

            glDepthFunc(GL_LEQUAL);
            
            glEnable(GL_BLEND);   
        }

        void Draw(float mirrorConstant) {
    
            glUseProgram(glProgram);

            UpdateUniformBlock();
            
            // glDisable(GL_CULL_FACE);
            // // glCullFace(GL_BACK);

            // glDepthFunc(GL_GEQUAL);
            // glClearDepthf(-1.f);
            // glClear(GL_DEPTH_BUFFER_BIT);

            // glDepthFunc(GL_LEQUAL);
            // glClearDepthf(1.f);
            // glClear(GL_DEPTH_BUFFER_BIT);

            // glEnable(GL_BLEND);
            // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);            

            
            //TODO: DEBUG CODE
            {
                // glUniform1f(UNIFORM_MIRROR_CONSTANT, mirrorConstant);
                // GlAssertNoError("Failed to set UNIFORM_MIRROR_CONSTANT");
    
                // Vec3<float> lightPosition = Vec3(10.f, 5.f, -10.f);
                // glUniform3fv(UNIFORM_LIGHT_POSITION, 1, lightPosition.component);
                // GlAssertNoError("Failed to set UNIFORM_LIGHT_POSITION");
            }
            
            //Note: no need to bind 'GL_ELEMENT_ARRAY_BUFFER', its part of vao state
            glBindVertexArray(vao);
            GlAssertNoError("Failed to bind vao");

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            GlAssertNoError("Failed to bind vbo");

            // TODO: Looks like glBindBufferBase doesn't save binding to glProgram? Is there a way to make this binding persist
            glBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_OBJECT, uniformObjectBlockBuffer);
            GlAssertNoError("Failed to bind uniformObjectBlockBuffer");

            glActiveTexture(GL_TEXTURE0+TU_SKY_MAP);
            glBindSampler(TU_SKY_MAP, skybox->CubeMapSampler());
            glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->CubeMapTexture());
            GlAssertNoError("Failed to set skybox texture");

            glActiveTexture(GL_TEXTURE0+TU_DEPTH_TEXTURE);
            glBindSampler(TU_DEPTH_TEXTURE, skybox->CubeMapDepthSampler());
            glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->CubeMapDepthTexture());
            GlAssertNoError("Failed to set skybox depth texture");

            glDrawElements(GL_TRIANGLES, numIndices, elementType, 0);
            GlAssertNoError("Failed to Draw");

            glClearDepthf(1.f);
            glDepthFunc(GL_LEQUAL);
            glCullFace(GL_BACK);
        }
        
        //TODO: create GENERIC!!! VBO
        //TODO: create VBI
        //TODO: create FBO pipeline: vbo queue -> render with their included shaders & optional VBI to backBuffer quad texture
        //      - VBO render pipeline takes in a camera to render to!
        //      - Camera system for handling MVP matrix transforms
        
        //TODO: create FBO
        //TODO: create COMPOSITE pipeline: fbo queue -> render with their included shaders (or none) -> stencil regions & z-ordering
        
        //TODO: have text render unit quad vertices for each character
        //      - write shader that positions text on
        //      - GlText is a type of renderable VBO - doesn't have world matrix though (default camera?)
        //      - GlText gets rendered to a hub FBO - can still add post processing like shake, and distortion!

};