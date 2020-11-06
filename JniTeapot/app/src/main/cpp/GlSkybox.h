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
            "   mat4 projectionMatrix;"
            "   mat4 cameraRotationMatrix;"
            "   mat4 cameraInverseRotationMatrix;"
            "};";
        
        static inline const StringLiteral kVertexShaderSourceDraw =
            ShaderVersionStr+
            ShaderIncludeQuaternion +
            kSkyBoxStr +
            ShaderOut(0) + "vec3 cubeCoord;" +
            ShaderOut(1) + "vec2 t;" +
            ShaderOut(2) + "vec4 q1;" +
            ShaderOut(3) + "vec4 q2;" +
            ShaderOut(4) + "vec4 q3;" +
            ShaderOut(5) + "vec3 p1;" +
            ShaderOut(6) + "vec3 p2;" +
            ShaderOut(7) + "vec3 p3;" +
            ShaderOut(8) + "vec3 p4;" +
            ShaderOut(9) + "vec3 p5;" +
            
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
                    
                    //TODO: get rid of inverses and just pass in uniform block when everythings done
                    //      Note: inverse(mat3(modelViewMatrix)) = transpose(mat3(cameraInverseRotation)) = mat3(cameraRotationMatrix)
                    vec4 clipCoords = vec4(vert, 1, 1.);
                    vec4 unprojected = inverse(projectionMatrix) * clipCoords;
                    cubeCoord = inverse(mat3(cameraInverseRotationMatrix)) * unprojected.xyz;
                    
                    //Note: flip x axis to translate right handed coordinates to left handed coordinates
                    //      opengl has negZ cubemap x-axis moving right to left
                    cubeCoord.x = -cubeCoord.x;
                    
                    //DEBUG
                    cubeCoord = mat3(cameraRotationMatrix) * normalize(vec3(vert, -1));
                    t = (.5*vert) + .5;
                    //t = vert;
                    p1 = mat3(cameraRotationMatrix) * normalize(vec3(-1,  0,  0));
                    p2 = mat3(cameraRotationMatrix) * normalize(vec3( 1,  0,  0));
                    
                    p3 = mat3(cameraRotationMatrix) * normalize(vec3( 0, -1,  0));
                    p4 = mat3(cameraRotationMatrix) * normalize(vec3( 0,  1,  0));
                    
                    p5 = mat3(cameraRotationMatrix) * normalize(vec3( 0,  0, -1));
                    
                    q1 = qRotateTo(vec3(-1,  0, 0), vec3(1, 0, 0));
                    q2 = qRotateTo(vec3( 0, -1, 0), vec3(0, 1, 0));
                    //q1 = qRotateTo(vec3(0, 0, -1), mat3(cameraRotationMatrix) * normalize(vec3(-1, -1, -1)));
                    //q2 = qRotateTo(normalize(vec3(-1, 0, 0)), normalize(vec3( 1, 0, 0)));
                    //q3 = qRotateTo(normalize(vec3(0, -1, 0)), normalize(vec3(0,  1, 0)));
            });
        
        static inline const StringLiteral kFragmentShaderSourceDraw =
            ShaderVersionStr +
            "precision highp float;"+
            
            ShaderIncludeQuaternion +
            kSkyBoxStr +

            ShaderSampler(TU_CUBE_MAP) + "samplerCube cubemap;" +
            ShaderIn(0)  + "vec3 cubeCoord;" +
            ShaderIn(1)  + "vec2 t;" +
            ShaderIn(2)  + "vec4 q1;" +
            ShaderIn(3)  + "vec4 q2;" +
            ShaderIn(4)  + "vec4 q3;" +
            ShaderIn(5)  + "vec3 p1;" +
            ShaderIn(6)  + "vec3 p2;" +
            ShaderIn(7)  + "vec3 p3;" +
            ShaderIn(8)  + "vec3 p4;" +
            ShaderIn(9)  + "vec3 p5;" +

            ShaderOut(0) + "vec4 fragColor;" +
            STRINGIFY(
                void main() {
                    fragColor = texture(cubemap, cubeCoord);
                    return;
                    
                    //vec3 direction = (p1*(1.-t.x)) + (p2*t.x) + //x
                    //                 (p3*(1.-t.y)) + (p4*t.y) + //y
                    //                 p5;                        //z
    

                    float halfTheta = .25 * Pi();
                    vec3 yAxis = vec3(0, 1, 0);
                    vec3 xAxis = vec3(1, 0, 0);
                    
                    vec3 direction;
                    //direction = qRotate(vec3(0, 0, -1),
                    //                    qMul(
                    //                         qSlerp(qFromAngle(-halfTheta, xAxis), qFromAngle( halfTheta, xAxis), t.y),
                    //                         qSlerp(qFromAngle( halfTheta, yAxis), qFromAngle(-halfTheta, yAxis), t.x)
                    //                    ));
                    
                    //direction.z = -1.;
    
                    
                    //vec3 eye = mat3(cameraRotationMatrix) * vec3(0, 0, -1);
                    direction.x = sqrt(2.) * sin(.25*Pi()*((2.*t.x)-1.));
                    direction.y = sqrt(2.) * sin(.25*Pi()*((2.*t.y)-1.));
                    //direction.z = -cos(.25*Pi() * ((2.*1)-1.));
                    direction.z = -1.;
                    
                                       
                    //direction = qRotate(direction,
                    //                    qSlerp(qIdentity(), qFromAngle(.5*Pi(), vec3(1, 0, 0)), t.y));
                    
                    //direction = mat3(cameraRotationMatrix) * (inverse(projectionMatrix) * vec4(direction, 1.)).xyz;
                    direction = mat3(cameraRotationMatrix) * direction;
                    
                    //vec3 direction = qRotate(vec3(0,  0,  -1), qSlerp(qIdentity(), qRotateTo(vec3(-1,  0, 0), vec3(1, 0, 0)), t.x)) + //x
                    //                 //qRotate(vec3( 0, -1,  0), qSlerp(qIdentity(), qRotateTo(vec3( 0, -1, 0), vec3(0, 1, 0)), t.y)) + //y
                    //                 //qRotate(vec3( 0, -1,  0), qSlerp(qIdentity(), q2, t.y)) + //y
                    //                 (p3*(1.-t.y)) + (p4*t.y) +
                    //                         vec3( 0,  0, -1);                                 //z
                    
                    //vec3 direction = qRotate(vec3(-1,-1,-1), rotation) + (p3*t.y);
                    float trans = .5;
                    vec3 coordColor = ((.5*direction) + .5);
                    //vec3 coordColor = ((.5*vec3(t, 0)) + .5);

                    if(t.x < .1 || t.x > .9) coordColor  = vec3(1., 0, 0);
                    if(t.y < .1 || t.y > .9) coordColor.bg =  vec2(1., 0);
                    if(abs(direction.x) < .9) coordColor*= .5;
                    if(abs(direction.y) < .9) coordColor*= .5;
                    
                    
                    fragColor = texture(cubemap, direction);
                    //fragColor = texture(cubemap, cubeCoord);
                    fragColor.rgb = (fragColor.rgb*trans) + ((1. - trans)*coordColor);
                };
            );
        
        static inline const StringLiteral kVertexShaderSourceWrite =
            ShaderVersionStr+
            kSkyBoxStr+
    
            ShaderOut(0) + "vec2 textureCoord;" +
            ShaderOut(1) + "flat int textureFace;" +
    
            //faceId's for GL_TEXTURE_CUBE_MAP_XXX
            "const int posZ = 4;"
            "const int negZ = 5;"
            
            "void main() {"
            
            //Note: texture coordinates have y-axis pointing down so we invert the
            //      cubes y-axis to make sure the vertex geometry matches the texture geometry
            "   const vec3[24] vertices = vec3[]("

            //posX
            "       vec3( 1,  1, -1),"
            "       vec3( 1,  1,  1),"
            "       vec3( 1, -1, -1),"
            "       vec3( 1, -1,  1),"
            
            //negX
            "       vec3(-1,  1,  1),"
            "       vec3(-1,  1, -1),"
            "       vec3(-1, -1,  1),"
            "       vec3(-1, -1, -1),"
            
            //posY
            "       vec3(-1,  1,  1),"
            "       vec3( 1,  1,  1),"
            "       vec3(-1,  1, -1),"
            "       vec3( 1,  1, -1),"
            
            //negY
            "       vec3(-1, -1, -1),"
            "       vec3( 1, -1, -1),"
            "       vec3(-1, -1,  1),"
            "       vec3( 1, -1,  1),"
            
            //posZ
            "       vec3(-1,  1, -1),"
            "       vec3( 1,  1, -1),"
            "       vec3(-1, -1, -1),"
            "       vec3( 1, -1, -1),"
            
            //negZ
            "       vec3( 1,  1,  1),"
            "       vec3(-1,  1,  1),"
            "       vec3( 1, -1,  1),"
            "       vec3(-1, -1,  1) "
            "   );"
            
            "   textureFace = gl_InstanceID;"
            "   vec3 vertex = vertices[gl_InstanceID*4 + gl_VertexID];"
            
            //TODO: make sure this operation is free. Just pulling single values from camera rotation matrix
            "   vec4 cameraX = cameraRotationMatrix * vec4( 1,  0,  0,  0);"
            "   vec4 cameraY = cameraRotationMatrix * vec4( 0, -1,  0,  0);"
            "   vec4 cameraZ = cameraRotationMatrix * vec4( 0,  0, -1,  0);"

            //"   vertex.y*= -1.;"
            
            "   gl_Position.x =  dot(vertex, cameraX.xyz);"
            "   gl_Position.y = -dot(vertex, cameraY.xyz);"
            "   gl_Position.z = -dot(vertex, cameraZ.xyz);"
            "   gl_Position.w = 1.;"

            "  gl_Position.z = gl_Position.z + 1.;"
            
            "   textureCoord = (.5*gl_Position.xy) + .5;"
            
            //"   gl_Position.z = gl_Position.z * (1./sqrt(3.));"
            //"   gl_Position.z = gl_Position.z * .5;"
            //"   gl_Position.xyz*= .5;"

            //Note: Shift z down to only in front of camera
            //"   gl_Position.z = gl_Position.z + 1.;"
            //"   gl_Position.z = (-2.*gl_Position.z) - 1.;"
            "   gl_Position.xyz = clamp(gl_Position.xyz, -1., 1.);"
            
            "   const vec3[6] uvFlips = vec3[]("
            "       vec3(1, 1, -1)," //posX
            "       vec3(1, 1, -1)," //negX
            "       vec3(1, 1, -1)," //posY
            "       vec3(1, 1, -1)," //negY
            "       vec3(1, 1, -1)," //posZ
            "       vec3(1, 1, -1) " //negZ
            "   );"
            
            "   gl_Position.xyz*= uvFlips[gl_InstanceID];"
            ""
            
            //"   gl_Position.z = ((1./sqrt(3.)) * gl_Position.z);"
            //"   gl_Position.xy = ((1./sqrt(3)) * gl_Position.xy);"
            //"   gl_Position.z = ((2./sqrt(3)) * gl_Position.z) - 1.;"
            //"   gl_Position.z = .5 * gl_Position.z;"


            // //THIS IS THE HARD CODED cube
            //"gl_Position.xy = vertex.xy;"
            //"gl_Position.z = -vertex.z;"

            "   gl_Position.x = (cameraInverseRotationMatrix * vec4(vertex, 0.)).x;"
            "   gl_Position.y = (cameraInverseRotationMatrix * vec4(vertex, 0.)).y;"
            "   gl_Position.z = (cameraInverseRotationMatrix * vec4(vertex, 0.)).z;"
            //"   gl_Position.xyz = clamp(gl_Position.xyz, -1., 1.);"
            //"   gl_Position.z = ((-2./sqrt(3)) * gl_Position.z) - 1.;"
            
            "   textureCoord = .5 * (gl_Position.xy + 1.);"
            
            "}";
        
        static inline const StringLiteral kFragmentShaderSourceWrite =
            ShaderVersionStr+
            ShaderExtension("GL_OES_EGL_image_external")+
            ShaderExtension("GL_OES_EGL_image_external_essl3") +
            "precision mediump float;" +

            ShaderSampler(TU_IMAGE) + "samplerExternalOES imageSampler;" +
            
            ShaderIn(0)  + "vec2 textureCoord;" +
            ShaderIn(1)  + "flat int textureFace;" +
            ShaderOut(0) + "vec4[6] fragColor;" +
            
            "void main() {"
            
             //RGBA = 0, 0, 0, 0
            "   fragColor[0] = vec4(0);"
            "   fragColor[1] = vec4(0);"
            "   fragColor[2] = vec4(0);"
            "   fragColor[3] = vec4(0);"
            "   fragColor[4] = vec4(0);"
            "   fragColor[5] = vec4(0);"

            ////Debug Colors
            //"   fragColor[0] = vec4(0., 1., float(textureFace)/6., 1.);"
            //"   fragColor[1] = vec4(0., 1., float(textureFace)/6., 1.);"
            //"   fragColor[2] = vec4(0., 1., float(textureFace)/6., 1.);"
            //"   fragColor[3] = vec4(0., 1., float(textureFace)/6., 1.);"
            //"   fragColor[4] = vec4(0., 1., float(textureFace)/6., 1.);"
            //"   fragColor[5] = vec4(0., 1., float(textureFace)/6., 1.);"

            "   fragColor[textureFace] = texture(imageSampler, textureCoord);"
            
            //"   fragColor[textureFace] = vec4(textureCoord, float(textureFace+1)/6., 1.);"
            
            //"   if(textureCoord.x == 0.) fragColor[textureFace]+= .001 *texture(imageSampler, textureCoord);"
            "}";
        
        struct alignas(16) UniformBlock {
            Mat4<float> projectionMatrix;
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
    
                ////TODO: CLEANUP
                //{
                //    Mat4<float> viewMatrix = camera->GetViewMatrix();
                //    viewMatrix.a4 = 0.f;
                //    viewMatrix.b4 = 0.f;
                //    viewMatrix.c4 = 0.f;
                //    viewMatrix.d4 = 1.f;
                //    //viewMatrix.column[3] = Vec4<float>(0.f, 0.f, 0.f, 0.f);
                //
                //    glm::mat4 glmViewMat = glm::make_mat4(viewMatrix.values);
                //    glm::mat4 glmInverseMatrix = glm::inverse(glmViewMat);
                //
                //    float* data = glm::value_ptr(glmInverseMatrix);
                //    Mat4<float> inverseViewMatrix(data);
                //
                //    uniformBlock->projectionMatrix = camera->GetProjectionMatrix();
                //    uniformBlock->cameraRotationMatrix = inverseViewMatrix;
                //    uniformBlock->cameraInverseRotationMatrix = viewMatrix;
                //}

                
                Quaternion<float> cameraRotation = camera->GetTransform().GetRotation();
                uniformBlock->cameraRotationMatrix = cameraRotation.Matrix();

                //TODO: might not need this once write program is fully implemented
                uniformBlock->cameraInverseRotationMatrix = cameraRotation.Conjugate().Matrix();
                uniformBlock->projectionMatrix = camera->GetProjectionMatrix();
                
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
        
        //Draws texture to cubemap were camera is currently pointing
        void UpdateTextureEGL(uint32 eglTexture, const GlContext* context) {
    
            //TODO: play around with this to see if we really want to change the viewport
            //Setup write framebuffer
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFrameBuffer);
            glViewport(0, 0, 200, 200);
    
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
            //if constexpr(false)
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
            
            //glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 6);
            //glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 3, 6);
    
            //Restore initial state
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
