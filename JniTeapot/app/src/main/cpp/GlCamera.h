#pragma once

#include "GlTransform.h"
#include "types.h"

class GlCamera {
    private:
        enum Flags { FLAG_PROJECTION_MATRIX_UPDATED = 1<<0, FLAG_CAM_TRANSFORM_UPDATED = 1<<1 };

        GlTransform transform;
        Mat4<float> projectionMatrix,
                    cameraMatrix,
                    viewMatrix;

        uint32 flags, matrixId;

        GLuint eglTexture;
        GLuint sampler;
        
    public:

        inline GlCamera(const Mat4<float>& projectionMatrix = Mat4<float>::identity, const GlTransform& transform = GlTransform()):
                transform(transform),
                projectionMatrix(projectionMatrix),
                flags(FLAG_PROJECTION_MATRIX_UPDATED|FLAG_CAM_TRANSFORM_UPDATED) {
    
            glGenTextures(1, &eglTexture);
            GlAssertNoError("Failed to generate eglCameraTexture");
    
            glGenSamplers(1, &sampler);
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            GlAssertNoError("Failed to create sampler");
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
        
        inline void SetTransform(const GlTransform& t)         { transform = t;         ++matrixId; flags|= FLAG_CAM_TRANSFORM_UPDATED; }
        inline void SetProjectionMatrix(const Mat4<float>& pm) { projectionMatrix = pm; ++matrixId; flags|= FLAG_PROJECTION_MATRIX_UPDATED; }

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

        //TODO: add Render() that can render objects and project them on to the EGLTexture
        
        //TODO: implement this to replace DrawCameraBackground in ARWrapper
        //void Draw() {
        //
        //        const static GLfloat kCameraVerts[] = {-1.0f, -1.0f,
        //                                               +1.0f, -1.0f,
        //                                               -1.0f, +1.0f,
        //                                               +1.0f, +1.0f };
        //
        //        //// TODO: this only needs to happen once unless the display geometry changes
        //        ////for(int i = 0; i < ArrayCount(kCameraVerts); ++i) transformedUVs[i] = .5f * (kCameraVerts[i] + 1.f);
        //        //ArFrame_transformCoordinates2d(arSession, arFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES, 4, kCameraVerts, AR_COORDINATES_2D_TEXTURE_NORMALIZED, transformedUVs);
        //
        //        glBindVertexArray(0);
        //
        //        glBindSampler(TU_BACKGROUND, 0);
        //        glActiveTexture(GL_TEXTURE0);
        //        glBindTexture(GL_TEXTURE_EXTERNAL_OES, eglTexture);
        //
        //        glUseProgram(glProgram);
        //
        //        //upload uniform matrix location --- only upload if updated
        //        glUniformMatrix4fv(cameraViewMatPosition, 1, GL_FALSE, cam.Matrix().values);
        //
        //        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        //
        //        GlAssertNoError("Error drawing camera background texture");
        //}
};