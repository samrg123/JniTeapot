#pragma once

//#include <glm.hpp>
//#include "glm/gtc/type_ptr.hpp"

#include "GlContext.h"
#include "GLES2/gl2ext.h"
#include "GlRenderable.h"

#include "FileManager.h"
#include "lodepng/lodepng.h"

#include "shaderUtil.h"
#include "mat.h"

class GlSkybox : public GlRenderable {
    private:
        
        enum TextureUnits  { TU_CUBE_MAP, TU_IMAGE = 0 };
        enum UniformBlocks { UBLOCK_SKY_BOX = 1 };
        
        static inline const StringLiteral kSkyBoxStr =
            ShaderUniformBlock(UBLOCK_SKY_BOX) + "SkyBox {"
            "   mat4 clipToWorldSpaceMatrix;" //inverse(viewMatrix)*inverse(projectionMatrix)
            "   mat4 cameraRotationMatrix;"
            "   mat4 cameraInverseRotationMatrix;"
            "   mat4 viewMatrix;"
            "};";
    
        static inline const StringLiteral kVertexShaderSourceDraw =
            ShaderVersionStr+
            kSkyBoxStr +
            ShaderOut(0) + "vec3 cubeCoord;" +

            STRINGIFY(
                void main() {
                    const vec2[4] vertices = vec2[](
                            vec2(-1., -1.),
                            vec2(-1.,  1.),
                            vec2( 1., -1.),
                            vec2( 1.,  1.)
                        );

                    vec2 vert = vertices[gl_VertexID];

                    //Note: opengl is left handed so we set depth to 1 (farthest away)
                    gl_Position = vec4(vert, 1., 1.);

                    cubeCoord = (clipToWorldSpaceMatrix * gl_Position).xyz;

                    //Note: flip x axis to translate right handed coordinates to left handed coordinates
                    //      opengl has negZ cubemap x-axis moving right to left
                    cubeCoord.x = -cubeCoord.x;
            });

        static inline const StringLiteral kFragmentShaderSourceDraw =
            ShaderVersionStr +
            "precision highp float;"+

            ShaderSampler(TU_CUBE_MAP) + "samplerCube cubemap;" +
            ShaderIn(0)  + "vec3 cubeCoord;" +

            ShaderOut(0) + "vec4 fragColor;" +
            STRINGIFY(
                void main() {
                    fragColor = texture(cubemap, cubeCoord);
                }
            );
        
        static inline const StringLiteral kVertexShaderSourceWrite =
            ShaderVersionStr+
            kSkyBoxStr+

            ShaderOut(0) + "flat int textureFace;" +
            ShaderOut(1) + "vec3 viewPos;"

            STRINGIFY(
                void main() {

                    vec2[4] verts = vec2[] (
                        vec2(-1.0f, -1.0f),
                        vec2(+1.0f, -1.0f),
                        vec2(-1.0f, +1.0f),
                        vec2(+1.0f, +1.0f)
                    );

                    vec2 a_Position = verts[gl_VertexID];
                    textureFace = gl_InstanceID;

                    //vec2 a_Position = verts[gl_VertexID%4];
                    //textureFace = gl_VertexID/4;

                    vec3 fakeWorldPos;

                    //X
                    if(textureFace == 0) { fakeWorldPos = vec3(1., -a_Position.y, -a_Position.x); }
                    if(textureFace == 1) { fakeWorldPos = vec3(-1., -a_Position.y, a_Position.x); }

                    //Y
                    if(textureFace == 2) { fakeWorldPos = vec3(a_Position.x, 1., a_Position.y); } // befre just -x
                    if(textureFace == 3) { fakeWorldPos = vec3(a_Position.x, -1., -a_Position.y); }

                    //z
                    if(textureFace == 4) { fakeWorldPos = vec3(a_Position.x, -a_Position.y, 1.); }
                    if(textureFace == 5) { fakeWorldPos = vec3(-a_Position.x, -a_Position.y, -1.); }

                    //reflect x axis to translate right handed world space into left handed cubemap space
                    fakeWorldPos.x = -fakeWorldPos.x;

                    viewPos = mat3(viewMatrix) * fakeWorldPos;

                    gl_Position = vec4(a_Position, 1., 1.);
                    //if(viewPos.x > -1. && viewPos.x < 1. &&
                    //   viewPos.y > -1. && viewPos.y < 1. && viewPos.z > 0.) {
                    //
                    //    vec2 cameraTexCoord = vec2((viewPos.x+1.)/2., (1.0-(viewPos.y+1.)/2.));
                    //    //fragColor[textureFace]=texture(imageSampler, cameraTexCoord);
                    //    fragColor[textureFace]= vec4(0, 0, 1, 1);
                    //} else {
                    //    fragColor[textureFace]=vec4(0.);
                    //}
                }
            );

        static inline const StringLiteral kFragmentShaderSourceWrite =
            ShaderVersionStr+
            ShaderExtension("GL_OES_EGL_image_external")+
            ShaderExtension("GL_OES_EGL_image_external_essl3") +
            "precision mediump float;" +

            ShaderSampler(TU_IMAGE) + "samplerExternalOES imageSampler;" +

            ShaderIn(0) + "flat int textureFace;" +
            ShaderIn(1) + "vec3 viewPos;" +

            ShaderOut(0) + "vec4[6] fragColor;" +

            STRINGIFY(
                void main() {

                    
                    //TODO: THIS IS SLOW ON MOBILE DUE TO EXTRA MEMORY WRITES JUST USE DUMB LOOP!!
                    fragColor[0] = vec4(0);
                    fragColor[1] = vec4(0);
                    fragColor[2] = vec4(0);
                    fragColor[3] = vec4(0);
                    fragColor[4] = vec4(0);
                    fragColor[5] = vec4(0);

                    if( viewPos.x > -1. && viewPos.x < 1. &&
                        viewPos.y > -1. && viewPos.y < 1. &&
                        viewPos.z > 0.) {

                        vec2 cameraTexCoord = vec2((viewPos.x+1.)/2., (1.0-(viewPos.y+1.)/2.));
                        fragColor[textureFace] = texture(imageSampler, cameraTexCoord);
                    }


                    //vec2 cameraTexCoord=vec2((viewPos.x+1.)/2., (1.0-(viewPos.y+1.)/2.));
                    //fragColor[textureFace]= vec4(0, 0, 1, 1);
                }
            );

        struct alignas(16) UniformBlock {
            Mat4<float> clipToWorldSpaceMatrix;
            Mat4<float> cameraRotationMatrix;
            Mat4<float> cameraInverseRotationMatrix;
            Mat4<float> viewMatrix;
        };

        GLuint glProgramDraw, glProgramWrite;
        GLuint writeFrameBuffer, uniformBuffer;
        
        GLuint sampler;

        GLuint texture;
        GLint textureSize;
        bool generateMipmaps;
        
        inline void UpdateMipMaps() {
            if(generateMipmaps) {
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
                GlAssertNoError("Failed to generate cubemap mipmaps");
            }
        }
        
        inline void UpdateUniformBlock() {
            if(CameraUpdated()) {
                ApplyCameraUpdate();

                glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer);
                UniformBlock* uniformBlock = (UniformBlock*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(UniformBlock), GL_MAP_WRITE_BIT);
                GlAssert(uniformBlock, "Failed to map uniformBlock");

                uniformBlock->clipToWorldSpaceMatrix = camera->GetTransform().GetRotation().Matrix() * camera->GetProjectionMatrix().Inverse();

                //TODO: might not need this once write program is fully implemented
                {
                    Quaternion<float> cameraRotation = camera->GetTransform().GetRotation();
                    uniformBlock->cameraRotationMatrix = cameraRotation.Matrix();

                    uniformBlock->cameraInverseRotationMatrix = cameraRotation.Conjugate().Matrix();
                    uniformBlock->viewMatrix = camera->Matrix();
                };

                glUnmapBuffer(GL_UNIFORM_BUFFER);
            }
        }

    public:
        struct SkyboxParams {
            union {
                struct {
                    const char  *posX,
                                *negX,
                                *posY,
                                *negY,
                                *posZ,
                                *negZ;
                };
                
                const char* images[6];
            };
            
            GlCamera* camera;
            bool generateMipmaps = false;
        };
        
        inline GLuint CubeMapSampler() const { return sampler; }
        inline GLuint CubeMapTexture() const { return texture; }
        
        GlSkybox(const SkyboxParams &params): GlRenderable(params.camera), generateMipmaps(params.generateMipmaps) {
    
            
            glProgramDraw  = GlContext::CreateGlProgram(kVertexShaderSourceDraw.str, kFragmentShaderSourceDraw.str);
            glProgramWrite = GlContext::CreateGlProgram(kVertexShaderSourceWrite.str, kFragmentShaderSourceWrite.str);
    
            glGenFramebuffers(1, &writeFrameBuffer);
            GlAssertNoError("Failed to create render buffer");
            
            glGenBuffers(1, &uniformBuffer);
            GlAssertNoError("Failed to create uniform buffer");
    
            glBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_SKY_BOX, uniformBuffer);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformBlock), nullptr, GL_DYNAMIC_DRAW);
            GlAssertNoError("Failed to bind uniform buffer to: %u", UBLOCK_SKY_BOX);
            
            glGenSamplers(1, &sampler);
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
            GlAssertNoError("Failed to create sampler: %u", sampler);
            
            glGenTextures(1, &texture);
            GlAssertNoError("Failed to create texture");
    
            //TODO: only do this once -- make an interface in glContext that we can query and doesn't rely on static
            int maxCubeMapSize;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxCubeMapSize);
            Log("maxCubeMapSize { %d }", maxCubeMapSize);
            
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
            
            Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
            
            for(int i = 0; i < ArrayCount(params.images); ++i) {
                
                FileManager::AssetBuffer* pngBuffer = FileManager::OpenAsset(params.images[i], &Memory::temporaryArena);
                
                uchar* bitmap;
                uint width, height;
                lodepng_decode_memory(&bitmap, &width, &height,
                                      pngBuffer->data, pngBuffer->size,
                                      LodePNGColorType::LCT_RGBA, 8);
                
                RUNTIME_ASSERT(width == height,
                              "Cubemap width must equal height. { side: %d, assetPath: '%s', width: %u, height: %u }",
                               i, params.images[i], width, height);
                
                RUNTIME_ASSERT(width < maxCubeMapSize,
                               "Cubemap width is too large { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                               maxCubeMapSize, i, params.images[i], width, height);

                RUNTIME_ASSERT(height < maxCubeMapSize,
                               "Cubemap height is too large { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                               maxCubeMapSize, i, params.images[i], width, height);

                RUNTIME_ASSERT(i == 0 || textureSize == width,
                              "Cubmap must be cube complete (all textures in cubemap have same dimensions). Current image doesn't match textureSize: %d "
                              "{ side: %d, assetPath: '%s', width: %u, height: %u }",
                              textureSize, i, params.images[i], width, height);
                
                textureSize = width;
                
                //TODO: pass in desired width and height so that we can use small initial texture
                //      but still render to the camera texture at camera resolution
                //      may require some software magnification/minification of initial texture?
                
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                             0,               //mipmap level
                             GL_RGBA,         //internal format
                             width,
                             height,
                             0,                //border must be 0
                             GL_RGBA,          //input format
                             GL_UNSIGNED_BYTE, //input type
                             bitmap);
                
                GlAssertNoError("Failed to set cubemap image { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                                maxCubeMapSize, i, params.images[i], width, height);
                
                free(bitmap);
                Memory::temporaryArena.FreeBaseRegion(tmpRegion);
    
                Log("Loaded cubemap { i: %d, size: %d, assetPath: %s }", i, textureSize, params.images[i]);
            }
    
            UpdateMipMaps();
        }
        
        ~GlSkybox() {
            glDeleteFramebuffers(1, &writeFrameBuffer);
            glDeleteBuffers(1, &uniformBuffer);
            glDeleteSamplers(1, &sampler);
            glDeleteTextures(1, &texture);
            glDeleteProgram(glProgramDraw);
        }
        
        //Draws texture to cubemap were camera is currently pointing
        void UpdateTexture(const GlContext* context) {
            
            //Setup write framebuffer
            glDisable(GL_DEPTH_TEST);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFrameBuffer);
            glViewport(0, 0, textureSize, textureSize);
    
            //bind each side of cubemap to unique color channel
            for(int i=0; i < 6; ++i) {
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                       GL_COLOR_ATTACHMENT0+i,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,
                                       texture, 0
                                      );
            }
    
            GlAssert(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER),
                     "Incomplete framebuffer?");
    
            //Debug colors
            if constexpr(false)
            {
                GLenum colorAttachments[] = {
                    GL_COLOR_ATTACHMENT0,
                    GL_COLOR_ATTACHMENT1,
                    GL_COLOR_ATTACHMENT2,
                    GL_COLOR_ATTACHMENT3,
                    GL_COLOR_ATTACHMENT4,
                    GL_COLOR_ATTACHMENT5
                };
                
                Vec4<float> colors[] = {
                    Vec4(1.f, 0.f, 0.f, 1.f) * Vec3<float>::one, //red     posX
                    Vec4(1.f, 1.f, 1.f, 1.f) * Vec3<float>::one, //white   negX    //Note: green reserved for frag shader
                    Vec4(0.f, 0.f, 1.f, 1.f) * Vec3<float>::one, //blue    posY
                    Vec4(1.f, 1.f, 0.f, 1.f) * Vec3<float>::one, //yellow  negY
                    Vec4(0.f, 1.f, 1.f, 1.f) * Vec3<float>::one, //cyan    posZ
                    Vec4(1.f, 0.f, 1.f, 1.f) * Vec3<float>::one, //pink    negZ
                };
                
                for(int i = 0; i < 6; ++i) {
                    glDrawBuffers(i+1, colorAttachments);
                    glClearColor(colors[i].x, colors[i].y, colors[i].z, colors[i].w);
                    glClear(GL_COLOR_BUFFER_BIT);
    
                    colorAttachments[i] = GL_NONE;
                }
            }
    
            
            //TODO: IS THIS SAVED IN FBO?
            GLenum colorAttachments[] = {
                GL_COLOR_ATTACHMENT0,
                GL_COLOR_ATTACHMENT1,
                GL_COLOR_ATTACHMENT2,
                GL_COLOR_ATTACHMENT3,
                GL_COLOR_ATTACHMENT4,
                GL_COLOR_ATTACHMENT5
            };
            glDrawBuffers(ArrayCount(colorAttachments), colorAttachments);
            
            glUseProgram(glProgramWrite);
            UpdateUniformBlock();
    
            glBindVertexArray(0);
    
            glBindSampler(TU_IMAGE, camera->EglTextureSampler());
            glActiveTexture(GL_TEXTURE0+TU_IMAGE);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, camera->EglTexture());
    
            //TODO: benchmark the performance of storing face index array in vertex shader
            //      and using it with glDrawArrars(GL_TRIANGLE_STRIP, 0, 4*6) [instancing might be slow when there isn't many vertices]
            //TODO: benchmark this against 6 draw calls so fragment shader doesn't need to do alpha blending [no idea how expensive alpha blending vs drawCalls is]
            //TODO: benchmark this against geometry shader that sets gl_Layer to right face of cubemap [geometry shaders used to be very slow... is this still the case?]
    
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 6);
            UpdateMipMaps();
    
            //Restore initial state
            glEnable(GL_DEPTH_TEST);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            
            GLenum backBuffer = GL_BACK;
            glDrawBuffers(1, &backBuffer);
            glViewport(0, 0, context->Width(), context->Height());
            
            GlAssertNoError("Failed to Update Cubemap texture");
        }
        
        void Draw() {
            
            glUseProgram(glProgramDraw);
            UpdateUniformBlock();
    
            glBindVertexArray(0);
            glBindSampler(TU_CUBE_MAP, sampler);
    
            glActiveTexture(GL_TEXTURE0 + TU_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
            
            //Note: vertices are computed in vertexShader
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            
            GlAssertNoError("Failed to Draw");
        }
};
