#pragma once

#include "GlTransform.h"
#include "types.h"
#include "GLES2/gl2ext.h"

class GlCamera {
    private:
        
        enum TextureUnits  { TU_EGLTexture = 0 };
        enum Uniforms { UProjectionMatrix = 0 };
        static inline const StringLiteral kVertexShaderSourceDraw =
            ShaderVersionStr+
    
            ShaderUniform(UProjectionMatrix)+"mat4 projectionMatrix;"+
            ShaderOut(0)+"vec2 textureCord;"+
    
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
    
                    float tx = (gl_VertexID&2) == 0 ? 1. : 0.;
                    float ty = (gl_VertexID&1) == 0 ? 1. : 0.;
                    
                    //TODO: FIX THIS... Camera should use projection matrix? Probably just pass through vect2 of aspect ratio!
                    textureCord = vec2(tx, ty) + .0001*projectionMatrix[0][0];
                    
                    //mat2 newProjMat = mat2(projectionMatrix[0][0], projectionMatrix[0][1],
                    //                       projectionMatrix[1][0], projectionMatrix[1][1]);
                    //textureCord = newProjMat * vec2(tx, ty);
                });
        
        static inline const StringLiteral kFragmentShaderSourceDraw =
            ShaderVersionStr +
            ShaderExtension("GL_OES_EGL_image_external")+
            ShaderExtension("GL_OES_EGL_image_external_essl3")+
            
            "precision highp float;"+
            ShaderSampler(TU_EGLTexture) + "samplerExternalOES sampler;" +
            ShaderIn(0) + "vec2 textureCord;" +
            ShaderOut(0) + "vec4 fragColor;" +
            STRINGIFY(
                void main() {
                    fragColor = texture(sampler, textureCord);
                }
             );
        
        enum Flags { FLAG_PROJECTION_MATRIX_UPDATED = 1<<0, FLAG_CAM_TRANSFORM_UPDATED = 1<<1 };

        GlTransform transform;
        Mat4<float> projectionMatrix,
                    cameraMatrix,
                    viewMatrix;

        uint32 flags, matrixId;
        
        GLuint glProgramDraw;
        GLuint eglTexture;
        GLuint sampler;
        
    public:
        inline ~GlCamera() {
            glDeleteSamplers(1, &sampler);
            glDeleteProgram(glProgramDraw);
        }
        
        inline GLuint EglTexture()        const { return eglTexture; }
        inline GLuint EglTextureSampler() const { return sampler; }
        
        inline GlTransform GetTransform()        const { return transform; }
        inline Mat4<float> GetProjectionMatrix() const { return projectionMatrix; }
        
        inline Mat4<float> GetViewMatrix() {
            if(flags & FLAG_CAM_TRANSFORM_UPDATED) {
                flags^= FLAG_CAM_TRANSFORM_UPDATED;
                viewMatrix = transform.InverseMatrix();
            }
            return viewMatrix;
        }
        
        inline void SetTransform(const GlTransform& t)  { transform = t;         ++matrixId; flags|= FLAG_CAM_TRANSFORM_UPDATED; }
        inline void SetProjectionMatrix(const Mat4<float>& pm) {
            projectionMatrix = pm;
            ++matrixId;
            flags|= FLAG_PROJECTION_MATRIX_UPDATED;

            glUseProgram(glProgramDraw);
            glUniformMatrix4fv(UProjectionMatrix, 1, GL_FALSE, projectionMatrix.values);
            GlAssertNoError("Failed to set project matrix");
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
        
        inline GlCamera(const Mat4<float>& projectionMatrix = Mat4<float>::identity, const GlTransform& transform = GlTransform()):
            transform(transform),
            flags(FLAG_PROJECTION_MATRIX_UPDATED|FLAG_CAM_TRANSFORM_UPDATED) {
            
            glProgramDraw  = GlContext::CreateGlProgram(kVertexShaderSourceDraw.str, kFragmentShaderSourceDraw.str);
            
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

        //TODO: add Render() that can render objects and project them on to the EGLTexture
        
        void Draw() {
    
            glBindSampler(TU_EGLTexture, sampler);
            glActiveTexture(GL_TEXTURE0 + TU_EGLTexture);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, eglTexture);

            glUseProgram(glProgramDraw);
    
            //Note: vertices are computed in vertexShader
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
            GlAssertNoError("Error drawing camera background texture");
        }
};