#pragma once

#include "GlContext.h"
#include "GLES2/gl2ext.h"
#include "GlRenderable.h"

#include "FileManager.h"
#include "lodepng/lodepng.h"

#include "mat.h"
#include "shaderUtil.h"

class GlSkybox : public GlRenderable {
    private:
        
        enum TextureUnits  { TU_CUBE_MAP, TU_IMAGE = 0 };
        enum UniformBlocks { UBLOCK_SKY_BOX = 1 };
        enum Uniforms { UNIFORM_BLUR_ID = 0 };
        
        static inline constexpr StringLiteral kShaderVersion = "310 es";

        static inline constexpr StringLiteral kShaderSkyBox = Shader(
           
            ShaderUniformBlock(UBLOCK_SKY_BOX) SkyBox {
                mat4 clipToWorldSpaceMatrix; //inverse(viewMatrix)*inverse(projectionMatrix)
                mat4 cameraRotationMatrix;
                mat4 cameraInverseRotationMatrix;
                mat4 viewMatrix;
            };
        );

        struct alignas(16) UniformBlock {
            Mat4<float> clipToWorldSpaceMatrix;
            Mat4<float> cameraRotationMatrix;
            Mat4<float> cameraInverseRotationMatrix;
            Mat4<float> viewMatrix;
        };        

        static inline constexpr StringLiteral kShaderGetVertexCoord2d = Shader(

            vec2 GetVertexCoord2d() {

                const vec2[4] vertices = vec2[] (
                    vec2(-1., -1.),
                    vec2(-1.,  1.),
                    vec2( 1., -1.),
                    vec2( 1.,  1.)
                );

                return vertices[gl_VertexID]; 
            }

        );

        static inline constexpr StringLiteral kShaderGetVertexCoord3d = Shader(

            vec2 GetVertexCoord3d() {

                const vec3[14] vertices = vec3[](
                    
                    // -Z
                    vec3(-1.,  1., -1.),
                    vec3( 1.,  1., -1.),
                    vec3(-1., -1., -1.),
                    vec3( 1., -1., -1.),

                    vec3( 1., -1.,  1.),

                    vec3( 1.,  1., -1.),
                    vec3( 1.,  1.,  1.),
                    vec3(-1.,  1., -1.),
                    vec3(-1.,  1.,  1.),
                    vec3(-1., -1., -1.),
                    vec3(-1., -1.,  1.),
                    vec3( 1., -1.,  1.),
                    vec3(-1.,  1.,  1.),
                    vec3( 1.,  1.,  1.)
                );
                return vertices[gl_VertexID]; 
            }

        );

        static inline constexpr StringLiteral kVertexShaderDraw = Shader(

            ShaderVersion(kShaderVersion)

            ShaderInclude(kShaderSkyBox)
            ShaderInclude(kShaderGetVertexCoord2d)

            ShaderOut(0) vec3 cubeCoord;
            
            void main() {

                vec2 vert = GetVertexCoord2d();

                //Note: opengl is left handed so we set depth to 1 (farthest away)
                gl_Position = vec4(vert, 1., 1.);

                cubeCoord = (clipToWorldSpaceMatrix * gl_Position).xyz;

                //Note: flip x axis to translate right handed coordinates to left handed coordinates
                //      opengl has negZ cubemap x-axis moving right to left
                cubeCoord.x = -cubeCoord.x;
            }
        );

        static inline constexpr StringLiteral kFragmentShaderDraw = Shader(

            ShaderVersion(kShaderVersion)

            precision highp float;

            ShaderSampler(TU_CUBE_MAP) samplerCube cubemap;
            ShaderIn(0) vec3 cubeCoord;

            ShaderOut(0) vec4 fragColor;
        
            void main() {
                //TODO: ShaderSelect here to toggle between debugging and not debugging
                fragColor = textureLod(cubemap, cubeCoord, 0.);
                fragColor.a = .5;

                // Render cubemap coordinates
                // fragColor.rgb = cubeCoord;
            }
        );


        //TODO: Convert this into a 6 pass render - one pass for each face of the cube. 
        //      This will minimize memory overhead and discarded fragments.
        //      It also simplifies the code
        static inline constexpr StringLiteral kVertexShaderWrite = Shader(

            ShaderVersion(kShaderVersion)

            ShaderInclude(kShaderSkyBox)

            ShaderOut(0) flat int textureFace;
            ShaderOut(1)  vec3 viewPos;

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
                if(textureFace == 2) { fakeWorldPos = vec3(a_Position.x, 1., a_Position.y); } // before just -x
                if(textureFace == 3) { fakeWorldPos = vec3(a_Position.x, -1., -a_Position.y); }

                //z
                if(textureFace == 4) { fakeWorldPos = vec3(a_Position.x, -a_Position.y, 1.); }
                if(textureFace == 5) { fakeWorldPos = vec3(-a_Position.x, -a_Position.y, -1.); }

                //reflect x axis to translate right handed world space into left handed cubemap space
                fakeWorldPos.x = -fakeWorldPos.x;
                
                viewPos = mat3(viewMatrix) * fakeWorldPos;

                //TODO: perspective projection of texture onto side of cubemap to prevent artifacts

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

        static inline constexpr StringLiteral kFragmentShaderWrite = Shader(

            ShaderVersion(kShaderVersion)

            ShaderExtension("GL_OES_EGL_image_external")
            ShaderExtension("GL_OES_EGL_image_external_essl3")

            precision mediump float;

            ShaderSampler(TU_IMAGE) samplerExternalOES imageSampler;

            ShaderIn(0) flat int textureFace;
            ShaderIn(1) vec3 viewPos;

            ShaderOut(0) vec4[6] fragColor;

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

        static inline constexpr StringLiteral kVertexShaderDrawDepthTexture = Shader(
            
            ShaderVersion(kShaderVersion)

            ShaderInclude(kShaderSkyBox)
            ShaderInclude(kShaderGetVertexCoord2d)

            ShaderOut(0) vec3 cubeCoord;
            
            void main() {

                vec2 vert = GetVertexCoord2d();
                gl_Position = vec4(vert, 1., 1.);

                cubeCoord = (clipToWorldSpaceMatrix * gl_Position).xyz;
                
                // vert/= 8.; //zoom 8x
                // cubeCoord = vec3(1., vert.y, vert.x);    //+x wall only
                // cubeCoord = vec3(-1., vert.y, vert.x);   //-x wall only
                // cubeCoord = vec3(vert.x, 1., vert.y);    //+y wall only
                // cubeCoord = vec3(vert.x, -1., vert.y);   //+y wall only
                // cubeCoord = vec3(vert.x, vert.y, 1.);    //+z wall only
                // cubeCoord = vec3(vert.x, vert.y, -1.);   //-z wall only
            }
        );

        static inline constexpr StringLiteral kFragmentShaderDrawDepthTexture = Shader(
            
            ShaderVersion(kShaderVersion)

            precision highp float;

            ShaderSampler(TU_CUBE_MAP) samplerCube cubemap;
            ShaderIn(0) vec3 cubeCoord;

            ShaderOut(0) vec4 fragColor;
        
            void main() {

                //TODO: ShaderSelect here to toggle between debugging and not debugging
                vec2 textureColor = textureLod(cubemap, cubeCoord, 0.).rg;

                fragColor = vec4(0., 0., 0., 1.);

                if(all(lessThan(abs(textureColor.rg - vec2(-1., 1.)), vec2(.00001)))) {
                    
                    fragColor.a = .5;
               
                } else {
                    
                    fragColor.a = .75;

                    if(any(greaterThan(abs(textureColor.rg), vec2(1., 1.)))) {
 
                        //Show blue to indicate out of bounds
                        fragColor.rgb = vec3(0., 0., 1.);
                    
                    } else {

                        //show scaled color
                        fragColor.rg = .5 * (textureColor.rg + 1.);
                    }
                }


                // fragColor.rg+= vec2(1., 1.);
                // fragColor.rg*= .5;

                // fragColor.rgb = cubeCoord.xyz;
                // fragColor.a = 1.;
            }
        );

        static inline constexpr StringLiteral kVertexShaderBlurDepthTexture = Shader(
            
            ShaderVersion(kShaderVersion)

            ShaderInclude(kShaderGetVertexCoord2d)

            ShaderOut(0) vec2 fragPosition;

            void main() {

                vec2 vert = GetVertexCoord2d();

                //Note: opengl is left handed so we set depth to 1 (farthest away)
                gl_Position = vec4(vert, 1., 1.);
            
                fragPosition = vert;
            }
        );

        static inline constexpr StringLiteral kFragmentShaderBlurDepthTexture = Shader(
            
            ShaderVersion(kShaderVersion)

            precision highp float;

            ShaderUniform(UNIFORM_BLUR_ID) int blurId;

            ShaderSampler(TU_CUBE_MAP) samplerCube cubemap;

            ShaderIn(0) vec2 fragPosition;
            
            ShaderOut(0) vec2 fragColor;
        
            void main() {
                
                // //sigma = 1
                // const float weights[5] = float[5](
                //     0.0544886845496429,
                //     0.244201342003233,
                //     0.402619946894247,
                //     0.244201342003233,
                //     0.0544886845496429
                // );

                //sigma = 1.5
                const float weights[9] = float[9](
                    0.00761441916929634,
                    0.0360749696891839,
                    0.109586081797814,
                    0.213444541943404,
                    0.266559974800603,
                    0.213444541943404,
                    0.109586081797814,
                    0.0360749696891839,
                    0.00761441916929634
                );
   
                const int kKernelSize = weights.length();
                const int kHalfKernelSize = kKernelSize/2; 

                int blurFace = blurId >> 1;
                vec2 blurDirection = ((blurId&1) == 0) ? vec2(0., 1.) : vec2(1., 0.); 

                const int kBlurLod = 0; 

                ivec2 textureSize = textureSize(cubemap, kBlurLod);
                vec2 pixelSize = 1. / vec2(float(textureSize.x), float(textureSize.y));

                vec2 blurColor = vec2(0.);
                for(int i = 0; i < kKernelSize; ++i) {

                    float offset =  float(i - kHalfKernelSize);
                    vec2 uv = fragPosition + (blurDirection * offset) * pixelSize;

                    vec3 cubeCoord = blurFace == 0 ? vec3( 1.,   -uv.y, -uv.x) : 
                                     blurFace == 1 ? vec3(-1.,   -uv.y,  uv.x) :
                                     blurFace == 2 ? vec3( uv.x,  1.,    uv.y) :
                                     blurFace == 3 ? vec3( uv.x, -1.,   -uv.y) :
                                     blurFace == 4 ? vec3( uv.x, -uv.y,  1.) :
                                                     vec3(-uv.x, -uv.y, -1.);

                    vec2 texel = textureLod(cubemap, cubeCoord, float(kBlurLod)).rg;
                    blurColor+= weights[i] * texel;
                }

                fragColor = blurColor;
            }
        );

        GLuint  glProgramDraw, 
                glProgramWrite,
                glBlurDepthTexture,
                glProgramDrawDepthTexture;

        GLuint writeFrameBuffer, uniformBuffer;
        
        GLuint sampler;

        union {
            GLuint textures[3];
            struct {
                GLuint colorTexture;
                GLuint depthColorTexture;
                GLuint depthTexture;
            };
        };

        GLint textureSize;
        bool generateMipmaps;

        inline void GenerateCubemapMipmap() {
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            GlAssertNoError("Failed to generate cubemap mipmaps");
        }

        inline void GenerateCubemapMipmap(GLint texture) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
            GenerateCubemapMipmap();
        }
        
        inline void UpdateUniformBlock() {

            glBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_SKY_BOX, uniformBuffer);
            
            if(CameraUpdated()) {
                ApplyCameraUpdate();

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
            struct Cubemap {
                const char  *posX,
                            *negX,
                            *posY,
                            *negY,
                            *posZ,
                            *negZ;
            };            
            
            union {
                Cubemap cubemap; 
                const char* cubemapImages[6];
            };
            
            GlCamera* camera;
            bool generateMipmaps = false;
        };
        
        inline GLuint CubeMapSampler() const { return sampler; }
        inline GLuint CubeMapTexture() const { return colorTexture; }

        inline GLuint CubeMapDepthSampler() const { return sampler; }
        inline GLuint CubeMapDepthTexture() const { return depthColorTexture; }   //TODO: REname / remove this function?     
        
        GlSkybox(const SkyboxParams &params)
        : GlRenderable(params.camera), generateMipmaps(params.generateMipmaps) {
    
            glProgramDraw             = GlContext::CreateGlProgram(kVertexShaderDraw, kFragmentShaderDraw);
            glProgramWrite            = GlContext::CreateGlProgram(kVertexShaderWrite, kFragmentShaderWrite);
            
            glBlurDepthTexture        = GlContext::CreateGlProgram(kVertexShaderBlurDepthTexture, kFragmentShaderBlurDepthTexture);
            glProgramDrawDepthTexture = GlContext::CreateGlProgram(kVertexShaderDrawDepthTexture, kFragmentShaderDrawDepthTexture);
            
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
            
            //TODO: THIS CAUSES glow around edges of 
            glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
            // glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            // glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            
            // glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, 0.f);
            // glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, 1.f);

            GlAssertNoError("Failed to create sampler: %u", sampler);

            glGenTextures(ArrayCount(textures), textures);
            GlAssertNoError("Failed to create textures");
    
            //TODO: only do this once -- make an interface in glContext that we can query and doesn't rely on static
            int maxCubeMapSize;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxCubeMapSize);
            Log("maxCubeMapSize { %d }", maxCubeMapSize);
                        
            Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
        
            for(int i = 0; i < ArrayCount(params.cubemapImages); ++i) {
                
                FileManager::AssetBuffer* pngBuffer = FileManager::OpenAsset(params.cubemapImages[i], &Memory::temporaryArena);
                
                uchar* bitmap;
                uint width, height;
                lodepng_decode_memory(&bitmap, &width, &height,
                                      pngBuffer->data, pngBuffer->size,
                                      LodePNGColorType::LCT_RGBA, 8);
                
                RUNTIME_ASSERT(width == height,
                              "Cubemap width must equal height. { side: %d, assetPath: '%s', width: %u, height: %u }",
                               i, params.cubemapImages[i], width, height);
                
                RUNTIME_ASSERT(width < maxCubeMapSize,
                               "Cubemap width is too large { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                               maxCubeMapSize, i, params.cubemapImages[i], width, height);

                RUNTIME_ASSERT(height < maxCubeMapSize,
                               "Cubemap height is too large { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                               maxCubeMapSize, i, params.cubemapImages[i], width, height);

                RUNTIME_ASSERT(i == 0 || textureSize == width,
                              "Cubmap must be cube complete (all textures in cubemap have same dimensions). Current image doesn't match textureSize: %d "
                              "{ side: %d, assetPath: '%s', width: %u, height: %u }",
                              textureSize, i, params.cubemapImages[i], width, height);
                
                textureSize = width;
                
                //TODO: pass in desired width and height so that we can use small initial texture
                //      but still render to the camera texture at camera resolution
                //      may require some software magnification/minification of initial texture?
                glBindTexture(GL_TEXTURE_CUBE_MAP, colorTexture);
                glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0,               //mipmap level
                    GL_RGBA8,        //internal format
                    width,
                    height,
                    0,                //border must be 0
                    GL_RGBA,          //input format
                    GL_UNSIGNED_BYTE, //input type
                    bitmap
                );

                //TODO: see if we split 32 bit float across 2 16 bit channels and still have mipmapping work
                //      (requires us to make an EGL 3.2 context)
                //      otherwise we need to implement our own mipmapping!
                //      (mipmapping only supports color renderable and texture filterable)

                //TODO: make sure we have extension GL_EXT_color_buffer_float enabled
                glBindTexture(GL_TEXTURE_CUBE_MAP, depthColorTexture);
                glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0,        //mipmap level
                    GL_RG32F, //internal format
                    width,
                    height,
                    0,        //border must be 0
                    GL_RG,    //input format
                    GL_FLOAT, //input type
                    nullptr   //input data
                );
                             
                glBindTexture(GL_TEXTURE_CUBE_MAP, depthTexture);
                glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0,                      //mipmap level
                    GL_DEPTH_COMPONENT32F,  //internal format //TODO: Make sure this works .. may need to use 24 bit instead
                    width,
                    height,
                    0,                      //border must be 0
                    GL_DEPTH_COMPONENT,     //input format
                    GL_FLOAT,               //input type
                    nullptr                 //input data
                ); 

                GlAssertNoError("Failed to set cubemap image { maxCubeMapSize: %d, side: %d, assetPath: '%s', width: %u, height: %u }",
                                maxCubeMapSize, i, params.cubemapImages[i], width, height);
                
                free(bitmap);
                Memory::temporaryArena.FreeBaseRegion(tmpRegion);
    
                Log("Loaded cubemap { i: %d, size: %d, assetPath: %s }", i, textureSize, params.cubemapImages[i]);
            }
    
            if(generateMipmaps) {
                
                //Generate depthColorTexture mipMap
                glBindTexture(GL_TEXTURE_CUBE_MAP, depthColorTexture);
                GenerateCubemapMipmap();
                
                //Generate colorTexture mipMap
                glBindTexture(GL_TEXTURE_CUBE_MAP, colorTexture);
                GenerateCubemapMipmap();
                
            } else {
                glBindTexture(GL_TEXTURE_CUBE_MAP, colorTexture);
            }

            // //create render buffer
            // //TODO: clean this up in destructor!
            // //TODO: each cubemap face should have its own depth renderbuffer! (so we can render more than 1 object at a time)
            // GLuint renderBuffer;
            // glGenRenderbuffers(1, &renderBuffer);
            // GlAssertNoError("Failed to create renderBuffer");
            
            // glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);

            // //TODO: Clipping doesn't work with GL_DEPTH_COMPONENT32F.... is there a reason why? Is it just unsupported by the EGL backed. why doesn't this give us an error then!
            // // glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, textureSize, textureSize);
            // glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, textureSize, textureSize);
            // GlAssertNoError("Failed to allocate renderBuffer storage");
            
            // glBindRenderbuffer(GL_RENDERBUFFER, 0);
            

            // //bind render buffer to fbo
            // glBindFramebuffer(GL_FRAMEBUFFER, writeFrameBuffer);
            // glFramebufferRenderbuffer(GL_FRAMEBUFFER,
            //                           GL_DEPTH_ATTACHMENT,
            //                           GL_RENDERBUFFER,      //must be GL_RENDERBUFFER
            //                           renderBuffer);  
            // GlAssertNoError("Failed to attach render buffer")

            // //bind default FBO
            // glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        
        ~GlSkybox() {
            glDeleteFramebuffers(1, &writeFrameBuffer);
            glDeleteBuffers(1, &uniformBuffer);
            glDeleteSamplers(1, &sampler);
            glDeleteTextures(sizeof(textures), textures);
            glDeleteProgram(glProgramDraw);

            glDeleteProgram(glProgramDrawDepthTexture);
        }
    
        //Draws texture to cubemap were camera is currently pointing
        void UpdateTexture(const GlContext* context) {
            
            //TODO: writeFrameBuffer doesn't have a depth buffer attached to it.
            //      in openGL4.0 this disables depth testing. Is this also the case
            //      in GLES3.1 if so we can remove this line
            glDisable(GL_DEPTH_TEST);

            //Setup write framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFrameBuffer);
            glViewport(0, 0, textureSize, textureSize);
    
            //bind each side of cubemap to unique color channel
            for(int i=0; i < 6; ++i) {
                glFramebufferTexture2D(
                    GL_DRAW_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0+i,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,
                    colorTexture, 0
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

            //update colorTexture mipmap
            if(generateMipmaps) {
                GenerateCubemapMipmap(colorTexture);
            }
    
            //Restore initial state
            glEnable(GL_DEPTH_TEST);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            
            GLenum backBuffer = GL_BACK;
            glDrawBuffers(1, &backBuffer);
            glViewport(0, 0, context->Width(), context->Height());
            
            GlAssertNoError("Failed to Update Cubemap texture");
        }
        
        inline void BindDepthTexture(uint8 cubemapFace) {

            glFramebufferTexture2D( 
                GL_DRAW_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + cubemapFace,
                depthColorTexture, 0
            );

            glFramebufferTexture2D( 
                GL_DRAW_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + cubemapFace,
                depthTexture, 0
            );
        }


        void ClearDepthTexture(const GlContext* context) {

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFrameBuffer);
            glViewport(0, 0, textureSize, textureSize);

            constexpr float kMinDepthColor = -1.f;
            glClearColor(
                kMinDepthColor,                 //depth 
                kMinDepthColor*kMinDepthColor,  //depthSquared
                0.,                             //not used 
                1.
            );

            constexpr float kMinDepth = 0.f;
            glClearDepthf(kMinDepth); 

            // Draw into colorAttachment0
            GLuint colorBuffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &colorBuffer);

            for(int i = 0; i < 6; ++i) {

                BindDepthTexture(i);

                constexpr bool debug = false;
                if constexpr(debug) {
                    
                    Vec4<float> colors[] = {
                        Vec4(1.f, 0.f, 0.f, 1.f) * Vec3<float>::one, //red        posX
                        Vec4(0.f, 1.f, 0.f, 1.f) * Vec3<float>::one, //green      negX
                        Vec4(1.f, 1.f, 0.f, 1.f) * Vec3<float>::one, //yellow     poyY
                        Vec4(.5f, 0.f, 0.f, 1.f) * Vec3<float>::one, //dim red    negY
                        Vec4(0.f, .5f, 0.f, 1.f) * Vec3<float>::one, //dim green  posZ
                        Vec4(.5f, .5f, 0.f, 1.f) * Vec3<float>::one, //dim yellow negZ
                    };
                    
                    glClearColor(colors[i].x, colors[i].y, colors[i].z, colors[i].w);
                }

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            glClearColor(0.f, 0.f, 0.f, 0.f);
            glClearDepthf(1.f);

            GLuint backBuffer = GL_BACK;
            glDrawBuffers(1, &backBuffer);
            
            glViewport(0, 0, context->Width(), context->Height());
        }

        void AntiAlisDepthBuffer(const GlContext* context, bool gaussianBlur = true) {

            if(gaussianBlur) {
                glUseProgram(glBlurDepthTexture);

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);

                glBindVertexArray(0);
                glBindSampler(TU_CUBE_MAP, sampler);
        
                glActiveTexture(GL_TEXTURE0 + TU_CUBE_MAP);
                glBindTexture(GL_TEXTURE_CUBE_MAP, depthColorTexture);

                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFrameBuffer);
                glViewport(0, 0, textureSize, textureSize);

                GLuint drawBuffer = GL_COLOR_ATTACHMENT0;
                glDrawBuffers(1, &drawBuffer);

                for(int i = 0; i < 12; ++i) {

                    //TODO: make this a function we can call into. We use it in a lot of places
                    glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER,
                                            drawBuffer,
                                            GL_TEXTURE_CUBE_MAP_POSITIVE_X + (i>>1),
                                            depthColorTexture, 0
                                        );

                    glUniform1i(UNIFORM_BLUR_ID, i);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    GlAssertNoError("Failed to blur depthColorTexture. blurId: %d", i);
                }

                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

                GLuint backBuffer = GL_BACK;
                glDrawBuffers(1, &backBuffer);

                glViewport(0, 0, context->Width(), context->Height());
                
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
            }

            //generate mipmaps
            glBindTexture(GL_TEXTURE_CUBE_MAP, depthColorTexture);
            GenerateCubemapMipmap();
        }

        //TODO: cleanup
        void DrawDepthTexture() {
            
            glUseProgram(glProgramDrawDepthTexture);
            
            UpdateUniformBlock();
    
            glBindVertexArray(0);
            glBindSampler(TU_CUBE_MAP, sampler);

            glActiveTexture(GL_TEXTURE0 + TU_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, depthColorTexture);
            
            //Note: vertices are computed in vertexShader
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            
            GlAssertNoError("Failed to Draw Depth Texture");
        }

        //TODO: Implement GlFbo

        void AttachFBO() const {
            glBindFramebuffer(GL_FRAMEBUFFER, writeFrameBuffer);
            glViewport(0, 0, textureSize, textureSize);
        }

        void DetachFBO(const GlContext* context) const {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, context->Width(), context->Height());
        }
    
        void Draw() {
        
            glUseProgram(glProgramDraw);
            UpdateUniformBlock();
    
            glBindVertexArray(0);
            glBindSampler(TU_CUBE_MAP, sampler);

            glActiveTexture(GL_TEXTURE0 + TU_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, colorTexture);

            //Note: vertices are computed in vertexShader
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            
            GlAssertNoError("Failed to Draw");
        }
};
