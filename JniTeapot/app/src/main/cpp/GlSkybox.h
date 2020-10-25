#pragma once

#include "GlContext.h"
#include "GlRenderable.h"

#include "FileManager.h"
#include "lodepng/lodepng.h"

#include "shaderUtil.h"
#include "mat.h"

class GlSkybox : public GlRenderable {
    private:
        
        enum TextureUnits  { TU_CUBE_MAP };
        enum UniformBlocks { UBLOCK_SKY_BOX = 1 };
        
        static inline const StringLiteral kVertexShaderSource =
            kGlesVersionStr +
            ShaderUniformBlock(UBLOCK_SKY_BOX) + "SkyBox {"
            "   mat4 projectionMatrix;"
            "};" +
            ShaderOut(0) + "vec3 uvCoord;" +

            //TODO: THIS UNIFORM BINDING IS WEIRD AND LOWER RIGHT VERTEX IS WRONG!
            //TODO: see if computing vertex via id is actually faster than using VBO
            "void main() {"
            ""
            "    if(gl_VertexID < 4) {"
            "        uvCoord.x = ((gl_VertexID & 0x1) == 1) ?  1. : -1.;"
            "        uvCoord.y = ((gl_VertexID & 0x3) == 1) ? -1. :  1.;"
            "        uvCoord.z = -1.;"
            ""
            "    } else if(gl_VertexID < 10) {"
            "        uvCoord.x = (gl_VertexID < 7) ?  1. : -1.;"
            "        uvCoord.y = (gl_VertexID == 4 || gl_VertexID == 9) ? -1. :  1.;"
            "        uvCoord.z = ((gl_VertexID & 0x1) == 1) ? -1. :  1.;"
            ""
            "    } else {"
            "        uvCoord.x = ((gl_VertexID & 0x1) == 1) ? 1. : -1.;"
            "        uvCoord.y = (gl_VertexID < 12) ? -1. : 1.;"
            "        uvCoord.z = 1.;"
            "    }"
            ""
            "   gl_Position = projectionMatrix * .25 *vec4(uvCoord.xyz, 0.);"
            "   gl_Position.w = 1.;"
            "}";
        
        static inline const StringLiteral kFragmentShaderSource =
            kGlesVersionStr +
            "precision mediump float;" +

            ShaderSampler(TU_CUBE_MAP) + "samplerCube cubemap;" +
            ShaderIn(0) + "vec3 uvCoord;" +
            ShaderOut(0) + "vec4 fragColor;" +
    
            "void main() {"
            "  fragColor = vec4(uvCoord.xyz, 1.);"
            "}";
        
        struct alignas(16) UniformBlock {
            Mat4<float> mvpMatrix;
        };
        
        GLuint glProgram;
        GLuint uniformBuffer;
        GLuint sampler;
        GLuint texture;
    
    
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
            bool generateMipmaps = true;
        };
        
        GlSkybox(const SkyboxParams &params): GlRenderable(params.camera) {
            
            glProgram = GlContext::CreateGlProgram(kVertexShaderSource.str, kFragmentShaderSource.str);
            
            GlContext::PrintVariables(glProgram);
            
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
            
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
            
            Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
            for(int i = 0; i < ArrayCount(params.images); ++i) {
                
                FileManager::AssetBuffer* pngBuffer = FileManager::OpenAsset(params.images[i], &Memory::temporaryArena);
                
                uchar* bitmap;
                uint width, height;
                lodepng_decode_memory(&bitmap, &width, &height,
                                      pngBuffer->data, pngBuffer->size,
                                      LodePNGColorType::LCT_RGBA, 8);
                
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                             0,               //mipmap level
                             GL_RGBA,         //internal format
                             width,
                             height,
                             0,                //border must be 0
                             GL_RGBA,          //input format
                             GL_UNSIGNED_BYTE, //input type
                             bitmap);
                
                GlAssertNoError("Failed to set cubemap image { side: %d, assetPath: '%s', width: %u, height: %u }",
                                i, params.images[i], width, height);
                
                free(bitmap);
                Memory::temporaryArena.FreeBaseRegion(tmpRegion);
            }
            
            if(params.generateMipmaps) glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        }
        
        ~GlSkybox() {
            glDeleteBuffers(1, &uniformBuffer);
            glDeleteSamplers(1, &sampler);
            glDeleteTextures(1, &texture);
            glDeleteProgram(glProgram);
        }
        
        void Draw() {
            
            glUseProgram(glProgram);
            glBindSampler(TU_CUBE_MAP, sampler);
            glBindVertexArray(0);
            
            if(DidCameraUpdate()) {
                ApplyCameraUpdate();
                
                glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer);
                UniformBlock* uniformBlock = (UniformBlock*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(UniformBlock), GL_MAP_WRITE_BIT);
                GlAssert(uniformBlock, "Failed to map uniformBlock");

                uniformBlock->mvpMatrix = camera->Matrix();

                glUnmapBuffer(GL_UNIFORM_BUFFER);
            }
            
            glActiveTexture(GL_TEXTURE0 + TU_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
            
            //Note: vertices are computed in vertexShader
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 14);
            
            GlAssertNoError("Failed to Draw");
        }
};
