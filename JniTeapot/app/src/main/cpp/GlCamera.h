#pragma once

#include "GlTransform.h"
#include "types.h"

class GlCamera {
    private:
        enum Flags { FLAG_PROJECTION_MATRIX_UPDATED = 1<<1, FLAG_CAM_TRANSFORM_UPDATED = 1<<2 };
        
        GlTransform transform;
        Mat4<float> projectionMatrix,
                    viewMatrix, transformMatrix;
        
        uint32 flags, matrixId;
        
    public:
        inline GlCamera(const Mat4<float>& projectionMatrix, const GlTransform& transform = GlTransform()):
            transform(transform),
            projectionMatrix(projectionMatrix),
            flags(FLAG_PROJECTION_MATRIX_UPDATED|FLAG_CAM_TRANSFORM_UPDATED) {}
            
        inline GlTransform GetTransform() const { return transform; }
        inline void SetTransform(const GlTransform& t) { transform = t; ++matrixId; flags|= FLAG_CAM_TRANSFORM_UPDATED; }

        inline Mat4<float> GetProjectionMatrix() { return projectionMatrix; }
        inline void SetProjectionMatrix(const Mat4<float>& pm) { projectionMatrix = pm; ++matrixId; flags|= FLAG_PROJECTION_MATRIX_UPDATED; }

        //Note: monotonic increasing number that refers to current Matrix uid (ids wrap every 2^32 matrices)
        inline uint32 MatrixId() { return matrixId; }
        
        inline Mat4<float> Matrix() {
            
            if(flags & (FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED)) {
                
                if(flags & FLAG_CAM_TRANSFORM_UPDATED) {
                    transformMatrix = transform.Matrix();
                }
                
                flags&= ~(FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED);
                viewMatrix = projectionMatrix * transformMatrix;
            }
        
            return viewMatrix;
        }
};