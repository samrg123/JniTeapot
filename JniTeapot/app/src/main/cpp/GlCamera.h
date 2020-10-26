
#pragma once

#include "GlTransform.h"
#include "types.h"

class GlCamera {
    private:
        enum Flags { FLAG_PROJECTION_MATRIX_UPDATED = 1<<1, FLAG_CAM_TRANSFORM_UPDATED = 1<<2, FLAG_CAM_TRANSFORM_MATRIX_UPDATED = 1<<3 };
        
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
        inline void SetTransformMatrix(const Mat4<float>& tm) { transformMatrix = tm; ++matrixId; flags |= FLAG_CAM_TRANSFORM_MATRIX_UPDATED; }
        
        inline Mat4<float> GetProjectionMatrix() { return projectionMatrix; }
        inline void SetProjectionMatrix(const Mat4<float>& pm) { projectionMatrix = pm; ++matrixId; flags |= FLAG_PROJECTION_MATRIX_UPDATED; }
        
        //Note: monotonic increasing number that refers to current Matrix uid (ids wrap every 2^32 matrices)
        inline uint32 MatrixId() { return matrixId; }
        
        inline Mat4<float> Matrix() {
            
            if(flags & (FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED | FLAG_CAM_TRANSFORM_MATRIX_UPDATED)) {
                
                if(flags & FLAG_CAM_TRANSFORM_UPDATED) {
                    
                    //Note: column4 has x,y,z translation negated because camera shifts world in opposite direction
                    transformMatrix = transform.Matrix();
                    ((Vec3<float>&)transformMatrix.column[3])*= -1.f;
                }
                
                flags&= ~(FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED);
                viewMatrix = projectionMatrix * transformMatrix;
            }
            
            return viewMatrix;
        }
};

//#pragma once
//
//#include "GlTransform.h"
//#include "types.h"
//
//class GlCamera {
//    private:
//        enum Flags { FLAG_PROJECTION_MATRIX_UPDATED = 1<<1, FLAG_CAM_TRANSFORM_UPDATED = 1<<2, FLAG_CAM_VIEW_MAT_UPDATED = 1<<3 };
//
//        GlTransform transform;
//        Mat4<float> projectionMatrix,
//                    cameraMatrix, viewMatrix;
//
//        uint32 flags, matrixId;
//
//    public:
//        inline GlCamera(const Mat4<float>& projectionMatrix, const GlTransform& transform = GlTransform()):
//            transform(transform),
//            projectionMatrix(projectionMatrix),
//            flags(FLAG_PROJECTION_MATRIX_UPDATED|FLAG_CAM_TRANSFORM_UPDATED) {}
//
//        inline GlTransform GetTransform() const { return transform; }
//        inline void SetTransform(const GlTransform& t) { transform = t; ++matrixId; flags|= FLAG_CAM_TRANSFORM_UPDATED; }
//
//        inline Mat4<float> GetProjectionMatrix() { return projectionMatrix; }
//        inline void SetProjectionMatrix(const Mat4<float>& pm) { projectionMatrix = pm; ++matrixId; flags |= FLAG_PROJECTION_MATRIX_UPDATED; }
//        inline void SetViewMatrix(const Mat4<float>& m) { viewMatrix = m; ++matrixId; flags|= FLAG_CAM_VIEW_MAT_UPDATED; }
//
//        //Note: monotonic increasing number that refers to current Matrix uid (ids wrap every 2^32 matrices)
//        inline uint32 MatrixId() { return matrixId; }
//
//        inline Mat4<float> Matrix() {
//
//            cameraMatrix = projectionMatrix * viewMatrix;
//            return cameraMatrix;
//
//            //if(flags & (FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED)) {
//            //    if(flags & FLAG_CAM_TRANSFORM_UPDATED) {
//            //
//            //        //TODO: do we invert rotation?
//            //
//            //        //Note: column4 has x,y,z translation negated because camera shifts world in opposite direction
//            //        viewMatrix = transform.Matrix();
//            //        ((Vec3<float>&)viewMatrix.column[3])*= -1.f;
//            //    }
//            //
//            //    flags&= ~(FLAG_PROJECTION_MATRIX_UPDATED | FLAG_CAM_TRANSFORM_UPDATED);
//            //    cameraMatrix =projectionMatrix*viewMatrix;
//            //}
//
//            //return cameraMatrix;
//        }
//};