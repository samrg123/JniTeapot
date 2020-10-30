#pragma once

#include "GlTransform.h"
#include "types.h"

class GlCamera {
    private:
        enum Flags { FLAG_PROJECTION_MATRIX_UPDATED = 1<<1, FLAG_CAM_TRANSFORM_UPDATED = 1<<2,
                     FLAG_VIEW_MATRIX_SET = 1<<3 //TODO: THIS IS ONLY A TEMPORARY HACK TO GET ARCORE TO PLAY NICE!
        };

        GlTransform transform;
        Mat4<float> projectionMatrix,
                    cameraMatrix, viewMatrix;

        uint32 flags, matrixId;

    public:

        inline GlCamera(const Mat4<float>& projectionMatrix, const GlTransform& transform = GlTransform()):
                transform(transform),
                projectionMatrix(projectionMatrix),
                flags(FLAG_PROJECTION_MATRIX_UPDATED|FLAG_CAM_TRANSFORM_UPDATED) {}

                
        inline GlTransform GetTransform()        const { return transform; }
        inline Mat4<float> GetViewMatrix()       const { return viewMatrix; }
        inline Mat4<float> GetProjectionMatrix() const { return projectionMatrix; }
        
        inline void SetTransform(const GlTransform& t)         { transform = t; ++matrixId; flags|= FLAG_CAM_TRANSFORM_UPDATED; }
        inline void SetProjectionMatrix(const Mat4<float>& pm) { projectionMatrix = pm; ++matrixId; flags|= FLAG_PROJECTION_MATRIX_UPDATED; }
        inline void SetViewMatrix(const Mat4<float>& m)        { viewMatrix = m;        ++matrixId; flags|= FLAG_VIEW_MATRIX_SET; }

        //Note: monotonic increasing number that refers to current Matrix uid (ids wrap every 2^32 matrices)
        inline uint32 MatrixId() { return matrixId; }

        inline Mat4<float> Matrix() {

            // TODO - HACK TO GET ARCORE TO PLAY NICE - SHOULDN'T BE ABLE TO SPECIFY VIEWMATRIX!!
            if(flags&FLAG_VIEW_MATRIX_SET) cameraMatrix = projectionMatrix*viewMatrix;
            else {
    
                if(flags & (FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED)) {

                    if(flags & FLAG_CAM_TRANSFORM_UPDATED) viewMatrix = transform.InverseMatrix();

                    flags&= ~(FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED);
                    cameraMatrix = projectionMatrix*viewMatrix;
                }
            }
            return cameraMatrix;
        }
};