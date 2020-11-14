#pragma once

#include <glm.hpp>
#include "glm/gtc/type_ptr.hpp"

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
            "};";
        
        
        int cubemapCameraTextureUniform = 0;
        int cubemapFaceNumberUniform = 1;
        int cubemapScreenPositionAttrib = 0;
        int cubemapCameraMatrixUniform = 2;
        
        static inline const StringLiteral kVertexShaderSourceWrite =
            ShaderVersionStr +
            " in layout(location=0) vec4 a_Position;"
             // "attribute vec4 a_worldPos;"
             "uniform layout(location = 2) highp mat4 m_viewMat;"
             "out vec4 viewPos;"
             "uniform layout(location = 1) int i_face;"
             "void main() {"
             "   gl_Position = a_Position;"
             "   vec4 fakeWorldPos;"

             //X
             "   if (i_face == 0) { fakeWorldPos = vec4(1., -a_Position.y, -a_Position.x, 0.); }"
             "   if (i_face == 1) { fakeWorldPos = vec4(-1., -a_Position.y, a_Position.x, 0.); }"
             
             //Y
             "   if (i_face == 2) { fakeWorldPos = vec4(a_Position.x, 1., a_Position.y, 0.); }" // befre just -x
             "   if (i_face == 3) { fakeWorldPos = vec4(a_Position.x, -1., -a_Position.y, 0.); }"

             //z
             "   if (i_face == 4) { fakeWorldPos = vec4(a_Position.x, -a_Position.y, 1., 0.); }"
             "   if (i_face == 5) { fakeWorldPos = vec4(-a_Position.x, -a_Position.y, -1., 0.); }"

             //Po
             "  fakeWorldPos.x = -fakeWorldPos.x;"
             
             "   viewPos = m_viewMat * vec4(fakeWorldPos.xyz, 0.);"
             "}";
        static inline const StringLiteral kFragmentShaderSourceWrite =
            ShaderVersionStr +
            ShaderExtension("GL_OES_EGL_image_external") +
            ShaderExtension("GL_OES_EGL_image_external_essl3") +
           ""
           "precision mediump float;"
           "uniform layout(location=0, binding = 0) samplerExternalOES sTexture;"
           "uniform layout(location=2) highp mat4 m_viewMat;"
           "in vec4 viewPos;"
           ""
           "out vec4 fragColor;"
           ""
           "void main() {"
           "    if (viewPos.x > -1. && viewPos.x < 1. && viewPos.y > -1. && viewPos.y < 1. && viewPos.z > 0.) {"
           "        vec2 cameraTexCoord = vec2((viewPos.x + 1.)/2., (1.0 - (viewPos.y + 1.)/2.));"
           "        fragColor = texture(sTexture, cameraTexCoord);"
           //"        fragColor = vec4(0, 0, 1, 1) + (texture(sTexture, cameraTexCoord)*.0001);"
           "    } else {"
           "        fragColor = vec4(0.);"
           "    }"
           // "    gl_FragColor += vec4(abs(viewPos.rgb), 1.0f);"
           "}";

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
        
        //static inline const StringLiteral kVertexShaderSourceWrite =
        //    ShaderVersionStr+
        //    kSkyBoxStr+
        //
        //    ShaderOut(0) + "vec2 textureCoord;" +
        //    ShaderOut(1) + "flat int textureFace;" +
        //
        //    ShaderOut(2) + "vec3 viewPos;"
        //
        //    STRINGIFY(
        //        void main() {
        //
        //            vec3 fakeWorldPos;
        //
        //            vec2[4] verts= vec2[](
        //                vec2(-1.0f, -1.0f),
        //                vec2(+1.0f, -1.0f),
        //                vec2(-1.0f, +1.0f),
        //                vec2(+1.0f, +1.0f)
        //            );
        //
        //            vec2 a_Position = verts[gl_VertexID];
        //
        //            if (gl_InstanceID == 0) { fakeWorldPos = vec3(1., -a_Position.y, -a_Position.x); }
        //            if (gl_InstanceID == 1) { fakeWorldPos = vec3(-1., -a_Position.y, a_Position.x); }
        //            if (gl_InstanceID == 2) { fakeWorldPos = vec3(a_Position.x, 1., a_Position.y); }
        //            if (gl_InstanceID == 3) { fakeWorldPos = vec3(a_Position.x, -1., -a_Position.y); }
        //            if (gl_InstanceID == 4) { fakeWorldPos = vec3(a_Position.x, -a_Position.y, 1.); }
        //            if (gl_InstanceID == 5) { fakeWorldPos = vec3(-a_Position.x, -a_Position.y, -1.); }
        //
        //            //viewPos = m_viewMat * vec4(fakeWorldPos.rgb, 0.);
        //            viewPos = mat3(cameraInverseRotationMatrix) * fakeWorldPos;
        //
        //            gl_Position= vec4(a_Position, 1, 1);
        //
        //            textureFace = gl_InstanceID;
        //        }
        //
        //        void main2() {
        //            //Note: texture coordinates have y-axis pointing down so we invert the
        //            //      cubes y-axis to make sure the vertex geometry matches the texture geometry
        //            const vec3[24] vertices = vec3[](
        //
        //                //posX
        //                vec3( 1,  1, -1),
        //                vec3( 1,  1,  1),
        //                vec3( 1, -1, -1),
        //                vec3( 1, -1,  1),
        //
        //                //negX
        //                vec3(-1,  1,  1),
        //                vec3(-1,  1, -1),
        //                vec3(-1, -1,  1),
        //                vec3(-1, -1, -1),
        //
        //                //posY
        //                vec3(-1,  1,  1),
        //                vec3( 1,  1,  1),
        //                vec3(-1,  1, -1),
        //                vec3( 1,  1, -1),
        //
        //                //negY
        //                vec3(-1, -1, -1),
        //                vec3( 1, -1, -1),
        //                vec3(-1, -1,  1),
        //                vec3( 1, -1,  1),
        //
        //                //posZ
        //                vec3(-1,  1, -1),
        //                vec3( 1,  1, -1),
        //                vec3(-1, -1, -1),
        //                vec3( 1, -1, -1),
        //
        //                //negZ
        //                vec3( 1,  1,  1),
        //                vec3(-1,  1,  1),
        //                vec3( 1, -1,  1),
        //                vec3(-1, -1,  1)
        //            );
        //
        //            textureFace = gl_InstanceID;
        //            vec3 vertex = vertices[gl_InstanceID*4 + gl_VertexID];
        //
        //            //TODO: make sure this operation is free. Just pulling single values from camera rotation matrix
        //            vec4 cameraX = cameraRotationMatrix * vec4( 1,  0,  0,  0);
        //            vec4 cameraY = cameraRotationMatrix * vec4( 0, -1,  0,  0);
        //            vec4 cameraZ = cameraRotationMatrix * vec4( 0,  0, -1,  0);
        //
        //            //   vertex.y*= -1.;
        //
        //            gl_Position.x =  dot(vertex, cameraX.xyz);
        //            gl_Position.y = -dot(vertex, cameraY.xyz);
        //            gl_Position.z = -dot(vertex, cameraZ.xyz);
        //            gl_Position.w = 1.;
        //
        //            gl_Position.z = gl_Position.z + 1.;
        //
        //            textureCoord = (.5*gl_Position.xy) + .5;
        //
        //            //   gl_Position.z = gl_Position.z * (1./sqrt(3.));
        //            //   gl_Position.z = gl_Position.z * .5;
        //            //   gl_Position.xyz*= .5;
        //
        //            //Note: Shift z down to only in front of camera
        //            //   gl_Position.z = gl_Position.z + 1.;
        //            //   gl_Position.z = (-2.*gl_Position.z) - 1.;
        //
        //            gl_Position.xyz = clamp(gl_Position.xyz, -1., 1.);
        //
        //            //const vec3[6] uvFlips = vec3[](
        //            //    vec3( 1,  1, -1), //posX
        //            //    vec3( 1, -1, -1), //negX
        //            //    vec3( 1,  1, -1), //posY
        //            //    vec3( 1,  1, -1), //negY
        //            //    vec3( 1,  1, -1), //posZ
        //            //    vec3( 1,  1, -1)  //negZ
        //            //);
        //            //
        //            //const int[6] faceFlips = int[](
        //            //    0, //posX
        //            //    5, //negX
        //            //    2, //posY
        //            //    3, //negY
        //            //    0, //posZ
        //            //    0  //negZ
        //            //);
        //
        //            //textureFace = faceFlips[gl_InstanceID];
        //            //vec3 uvFlip = uvFlips[gl_InstanceID];
        //
        //            if(gl_VertexID == 0) vertex = vec3(-1,  1, 1);
        //            if(gl_VertexID == 1) vertex = vec3( 1,  1, 1);
        //            if(gl_VertexID == 2) vertex = vec3(-1, -1, 1);
        //            if(gl_VertexID == 3) vertex = vec3( 1, -1, 1);
        //
        //            textureFace = 5; //negX
        //
        //            gl_Position = cameraInverseRotationMatrix * vec4(vertex, 0);
        //            //gl_Position.y = -gl_Position.y;
        //            //gl_Position.x = -gl_Position.x;
        //
        //
        //            gl_Position.xyz = clamp(gl_Position.xyz, -1., 1.);
        //            gl_Position.w = 1.;
        //
        //            //   gl_Position.xyz = clamp(gl_Position.xyz, -1., 1.);
        //            //   gl_Position.z = ((-2./sqrt(3)) * gl_Position.z) - 1.;
        //
        //            textureCoord = .5 * (gl_Position.xy + 1.);
        //        }
        //    );
        //
        //static inline const StringLiteral kFragmentShaderSourceWrite =
        //    ShaderVersionStr+
        //    ShaderExtension("GL_OES_EGL_image_external")+
        //    ShaderExtension("GL_OES_EGL_image_external_essl3") +
        //    "precision mediump float;" +
        //
        //    ShaderSampler(TU_IMAGE) + "samplerExternalOES imageSampler;" +
        //
        //    ShaderIn(0)  + "vec2 textureCoord;" +
        //    ShaderIn(1)  + "flat int textureFace;" +
        //    ShaderIn(2) + " vec3 viewPos;" +
        //
        //    ShaderOut(0) + "vec4[6] fragColor;" +
        //
        //    STRINGIFY(
        //        void main() {
        //
        //            fragColor[0] = vec4(0);
        //            fragColor[1] = vec4(0);
        //            fragColor[2] = vec4(0);
        //            fragColor[3] = vec4(0);
        //            fragColor[4] = vec4(0);
        //            fragColor[5] = vec4(0);
        //
        //
        //            if(viewPos.x > -1. && viewPos.x < 1. && viewPos.y > -1. && viewPos.y < 1. &&
        //               viewPos.z > 0.) {
        //                vec2 cameraTexCoord=vec2((viewPos.x+1.)/2., (1.0-(viewPos.y+1.)/2.));
        //                //fragColor[textureFace]=texture(imageSampler, cameraTexCoord);
        //                fragColor[textureFace]= vec4(0, 0, 1, 1);
        //            } else {
        //                fragColor[textureFace]=vec4(0.);
        //            }
        //
        //            //fragColor[textureFace] += vec4(abs(viewPos.rgb), 1.0f);
        //        }
        //
        //        void main2() {
        //
        //            //RGBA = 0, 0, 0, 0
        //            fragColor[0] = vec4(0);
        //            fragColor[1] = vec4(0);
        //            fragColor[2] = vec4(0);
        //            fragColor[3] = vec4(0);
        //            fragColor[4] = vec4(0);
        //            fragColor[5] = vec4(0);
        //
        //            ////Debug Colors
        //            //   fragColor[0] = vec4(0., 1., float(textureFace)/6., 1.);
        //            //   fragColor[1] = vec4(0., 1., float(textureFace)/6., 1.);
        //            //   fragColor[2] = vec4(0., 1., float(textureFace)/6., 1.);
        //            //   fragColor[3] = vec4(0., 1., float(textureFace)/6., 1.);
        //            //   fragColor[4] = vec4(0., 1., float(textureFace)/6., 1.);
        //            //   fragColor[5] = vec4(0., 1., float(textureFace)/6., 1.);
        //
        //            //   fragColor[textureFace] = texture(imageSampler, textureCoord);
        //
        //            fragColor[textureFace] = vec4(textureCoord, float(textureFace+1)/6., 1.);
        //
        //            //   if(textureCoord.x == 0.) fragColor[textureFace]+= .001 *texture(imageSampler, textureCoord);
        //        }
        //    );
        
        struct alignas(16) UniformBlock {
            Mat4<float> clipToWorldSpaceMatrix;
            Mat4<float> cameraRotationMatrix;
            Mat4<float> cameraInverseRotationMatrix;
        };
        
        GLuint glProgramDraw, glProgramWrite;
        GLuint writeFrameBuffer, uniformBuffer;
        GLuint sampler;
        
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
                };
                
                glUnmapBuffer(GL_UNIFORM_BUFFER);
            }
        }

    public:
        GLuint texture;
        
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
            bool generateMipmaps = true;
        };
        
        inline GLuint CubeMapSampler() const { return sampler; }
        inline GLuint CubeMapTexture() const { return texture; }
        
        GlSkybox(const SkyboxParams &params): GlRenderable(params.camera) {
    
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
            glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            GlAssertNoError("Failed to create sampler: %u", sampler);
            
            glGenTextures(1, &texture);
            GlAssertNoError("Failed to create texture");
    
            //TODO: only do this once -- make an interface in glContext
            int maxCubeMapSize;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxCubeMapSize);
    
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
                              "Cubemap width must equal height. { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                               maxCubeMapSize, i, params.images[i], width, height);
                
                RUNTIME_ASSERT(width < maxCubeMapSize,
                               "Cubemap width is too large { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                               maxCubeMapSize, i, params.images[i], width, height);

                RUNTIME_ASSERT(height < maxCubeMapSize,
                               "Cubemap height is too large { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                               maxCubeMapSize, i, params.images[i], width, height);
                
                
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
                
                Log("Cubemap Size { i: %d, w: %d, h: %d }", i, width, height);
                
                GlAssertNoError("Failed to set cubemap image { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                                maxCubeMapSize, i, params.images[i], width, height);
                
                free(bitmap);
                Memory::temporaryArena.FreeBaseRegion(tmpRegion);
            }
            
            if(params.generateMipmaps) glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        }
        
        ~GlSkybox() {
            glDeleteFramebuffers(1, &writeFrameBuffer);
            glDeleteBuffers(1, &uniformBuffer);
            glDeleteSamplers(1, &sampler);
            glDeleteTextures(1, &texture);
            glDeleteProgram(glProgramDraw);
        }

        
        //TODO: MERGE WITH UpdateTextureEGL2
        void UpdateTextureEGL(uint32 eglTexure, const GlContext* context) {
    
            GLuint cubemapFbo;
            glGenFramebuffers(1, &cubemapFbo);
            GlAssertNoError("Failed to generate fbo");
            
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cubemapFbo);
            GlAssertNoError("Failed to bind fbo");
    
            glViewport(0, 0, 1024, 1024);
            GlAssertNoError("Failed to set viewport");
            
            for(int iFace = 0; iFace < 6; ++iFace) {
                
                static bool first[6]={true, true, true, true, true, true};
                const static GLfloat kCameraVerts[]={-1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f,
                                                     +1.0f, +1.0f
                };
    
                //attach a texture image to a framebuffer object
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X+iFace,
                                       texture, 0
                                      );
                GLenum status=glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                GlAssertNoError("Error drawing cubemap faceFIRST TWO");
    
    
                if(status != GL_FRAMEBUFFER_COMPLETE) {
                    Log("Status error: %08x\n", status);
                }
    
                glDepthMask(GL_FALSE);
                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
    
                glBindTexture(GL_TEXTURE_EXTERNAL_OES, eglTexure);
                //use our shader to do thing
                glUseProgram(glProgramWrite);
    
                if(first[iFace]) {
                    glClearColor(0, 0, 0, 1);
                    glClear(GL_COLOR_BUFFER_BIT);
                    first[iFace]=false;
                }
    
                glUniform1i(cubemapCameraTextureUniform, 0);
    
                glUniform1i(cubemapFaceNumberUniform, iFace);
    
    
                glEnableVertexAttribArray(cubemapScreenPositionAttrib);
                glVertexAttribPointer(cubemapScreenPositionAttrib, 2, GL_FLOAT, false, 0, kCameraVerts);
    
                // glEnableVertexAttribArray(cubemapWorldPositionAttrib);
                
                Mat4<float> viewProj=camera->GetProjectionMatrix()*camera->GetViewMatrix();
                //Mat4<float> viewProj=camera->GetProjectionMatrix()*camera->GetTransform().GetRotation().Matrix();
                
                glUniformMatrix4fv(cubemapCameraMatrixUniform, 1, GL_FALSE, viewProj.values);
    
                GlAssertNoError("Error drawing before draw");
    
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                GlAssertNoError("Error drawing after draw");
    
    
                // glDisableVertexAttribArray(cubemapWorldPositionAttrib);
                //glDisableVertexAttribArray(cubemapScreenPositionAttrib);
    
                glUseProgram(0);
                glDepthMask(GL_TRUE);
    
    
                //DrawCameraBackground();
    
    
    
                // glClear(GL_COLOR_BUFFER_BIT);
                GlAssertNoError("Error drawing cCLEARED OR WHATEVER");
    
    
                //activate texture and bind it
                // glActiveTexture(GL_TEXTURE0);
                // glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.getCubeTexture());
    
                // glUseProgram(0);
    
                GlAssertNoError("Error drawing cubemap face");
            }
    
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            GlAssertNoError("Failed to bind default fbo");
            glViewport(0, 0, context->Width(), context->Height());
            GlAssertNoError("Failed to set default viewport");
        }
        
        //Draws texture to cubemap were camera is currently pointing
        void UpdateTextureEGL2(uint32 eglTexture, const GlContext* context) {
    
            //TODO: play around with this to see if we really want to change the viewport
            //Setup write framebuffer
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFrameBuffer);
            glViewport(0, 0, 1024, 1024);
            //glDepthRangef(1, 0);
    
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
            glDrawBuffers(6, colorAttachments);
            
            
            glUseProgram(glProgramWrite);
            UpdateUniformBlock();
    
            glBindVertexArray(0);
    
            glBindSampler(TU_IMAGE, sampler);
            glActiveTexture(GL_TEXTURE0+TU_IMAGE);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, eglTexture);
            
            //glDrawArrays(GL_TRIANGLE_STRIP, 0, 14);
            //glDrawArrays(GL_TRIANGLE_STRIP, 0, 5);
            
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 6);
            //glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 3, 6);
            //glDrawArraysInstanced(GL_TRIANGLE_STRIP, 20, 3, 1);
    
            //Restore initial state
            //glDepthRangef(0, 1);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            
            GLenum backBuffer = GL_BACK;
            glDrawBuffers(1, &backBuffer);
            glViewport(0, 0, context->Width(), context->Height());
            
            GlAssertNoError("Failed to Update Cubemap");
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