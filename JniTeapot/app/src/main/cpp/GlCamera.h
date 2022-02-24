#pragma once

#include "GlTransform.h"
#include "types.h"
#include "GLES2/gl2ext.h"

class GlCamera {
    private:
        
        enum TextureUnits  { TU_EGLTexture };
        enum Uniforms      { U_Scale };
        
        static inline constexpr StringLiteral kShaderVersion = "310 es"; 

        static inline constexpr StringLiteral kVertexShaderSourceDraw = Shader(

            ShaderVersion(kShaderVersion)
            
            ShaderUniform(U_Scale) vec2 scale;
            
            ShaderOut(0) vec2 textureCoord;

            void main() {

                const vec2[4] vertices = vec2[] (
                    vec2(-1., -1.),
                    vec2(-1.,  1.),
                    vec2( 1., -1.),
                    vec2( 1.,  1.)
                );
            
                vec2 vert = vertices[gl_VertexID];

                //Note: opengl is left handed so we set depth to 1 (farthest away)
                gl_Position = vec4(vert, 1., 1.);

                vec2 screenProjection = vert * scale;                
                textureCoord = (screenProjection + vec2(1., -1.)) * vec2(.5, -.5);
            }
        );

        static inline const StringLiteral kTest = Shader(( vec2(0., 0.) ));

        static inline const StringLiteral kFragmentShaderSourceDraw = Shader(
            
            ShaderVersion(kShaderVersion)

            ShaderExtension("GL_OES_EGL_image_external")
            ShaderExtension("GL_OES_EGL_image_external_essl3")

            precision highp float;

            ShaderSampler(TU_EGLTexture) samplerExternalOES sampler;
            
            ShaderIn(0) vec2 textureCord;

            ShaderOut(0) vec4 fragColor;
            
            void main() {
                
                ShaderSelect(0,
                    
                    // TODO: Get JniTeapot to handle nested shaders and then push metaProg, shaderUtil and vscode as
                    //       update ShaderSelect
                    Shader(
                        fragColor = texture(sampler, textureCord);
                        fragColor.a = 1.;
                    ),
                    
                    Shader(
                        fragColor = vec4(textureCord, 1., 1.);
                    )
                );
            }
        );
        
        enum Flags { FLAG_PROJECTION_MATRIX_UPDATED = 1<<0, FLAG_CAM_TRANSFORM_UPDATED = 1<<1 };

        GlTransform transform;
        Mat4<float> projectionMatrix,
                    cameraMatrix,
                    viewMatrix;

        uint32 flags, matrixId;

        GLuint glProgramDraw;
        GLuint sampler;

        //TODO: pull EGLTexture out into a sperate class so we can construct one and return it
        //      directly from ARWrapper!        
        GLuint eglTexture;
        int32  eglTextureWidth;
        int32  eglTextureHeight;
        uint32 eglTextureId;
        uint32 drawEglTextureId;

        //TODO: replace this with GlViewport once it's implemented and use this to restore framebuffer sizes
        //      instead of passing GlContext parameter to GlSkybox etc.
        int    viewportWidth;
        int    viewportHeight;
        uint32 viewportId;
        uint32 drawViewportId;

        void UpdateUniforms() {

            if(drawViewportId != viewportId || drawEglTextureId != eglTextureId) {

                //Note: scale from screen * scale from texture
                float xMultiplier = viewportWidth  * eglTextureHeight;
                float yMultiplier = viewportHeight * eglTextureWidth;

                Vec2<float> scale = Vec2<float>(xMultiplier, yMultiplier) / Max(xMultiplier, yMultiplier);
                
                glUniform2f(U_Scale, scale.x, scale.y);
                GlAssertNoError("Failed to upload U_Scale");
            
                drawViewportId = viewportId;
                drawEglTextureId = eglTextureId;
            }
        }

    public:


        //TODO: Move EGL Texture out into its own type
        inline GLuint EglTexture()        const { return eglTexture; }
        inline GLuint EglTextureSampler() const { return sampler; }
        
        void SetEglTextureSize(int32 width, int32 height) {
            eglTextureWidth = width;
            eglTextureHeight = height;
            ++eglTextureId;
        }

        inline GlTransform GetTransform()        const { return transform; }
        inline Mat4<float> GetProjectionMatrix() const { return projectionMatrix; }
        
        inline Mat4<float> GetViewMatrix() {
            if(flags & FLAG_CAM_TRANSFORM_UPDATED) {
                flags^= FLAG_CAM_TRANSFORM_UPDATED;
                viewMatrix = transform.InverseMatrix();
            }
            return viewMatrix;
        }
        
        inline void SetTransform(const GlTransform& t)  { 
            transform = t;         
            ++matrixId; 
            flags|= FLAG_CAM_TRANSFORM_UPDATED; 
        }

        inline void SetProjectionMatrix(const Mat4<float>& pm) {
            projectionMatrix = pm;
            ++matrixId;
            flags|= FLAG_PROJECTION_MATRIX_UPDATED;
        }

        //Note: monotonic increasing number that refers to current Matrix uid (ids wrap every 2^32 matrices)
        inline uint32 MatrixId() { return matrixId; }

        inline Mat4<float> Matrix() {

            if(flags & (FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED)) {

                if(flags & FLAG_CAM_TRANSFORM_UPDATED) viewMatrix = transform.InverseMatrix();

                flags&= ~(FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED);
                cameraMatrix = projectionMatrix*viewMatrix;
            }
            
            return cameraMatrix;
        }
        
        //TODO: Pass in viewport object!
        inline GlCamera(int viewportWidth, int viewportHeight, 
                        const Mat4<float>& projectionMatrix = Mat4<float>::identity, 
                        const GlTransform& transform = GlTransform())  
                        
            : transform(transform),
              flags(FLAG_PROJECTION_MATRIX_UPDATED|FLAG_CAM_TRANSFORM_UPDATED),
              viewportWidth(viewportWidth),
              viewportHeight(viewportHeight) {
            
            //TODO: have Id object Initialize these correctly 
            drawViewportId = viewportId-1;
            drawEglTextureId = eglTextureId-1;

            //TODO: have EglTexture do this init
            eglTextureHeight = 0;
            eglTextureWidth = 0;

            glProgramDraw  = GlContext::CreateGlProgram(kVertexShaderSourceDraw, kFragmentShaderSourceDraw);
            
            glGenTextures(1, &eglTexture);
            GlAssertNoError("Failed to generate eglCameraTexture");
            
            glGenSamplers(1, &sampler);
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            GlAssertNoError("Failed to create sampler");
            
            SetProjectionMatrix(projectionMatrix);
        }

        inline ~GlCamera() {
            glDeleteSamplers(1, &sampler);
            glDeleteProgram(glProgramDraw);
        }        

        //TODO: add Render() that can render objects and project them on to the EGLTexture
        
        void Draw() {    
            
            glEnable(GL_BLEND);
            glDisable(GL_DEPTH_TEST);

            glBindSampler(TU_EGLTexture, sampler);
            glActiveTexture(GL_TEXTURE0 + TU_EGLTexture);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, eglTexture);

            glUseProgram(glProgramDraw);
    
            //Note: must be called after glUseProgram
            UpdateUniforms();

            //Note: vertices are computed in vertexShader
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
            GlAssertNoError("Error drawing camera background texture");

            glEnable(GL_DEPTH_TEST);
        }
};