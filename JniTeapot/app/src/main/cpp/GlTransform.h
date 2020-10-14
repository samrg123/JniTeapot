#pragma once
#include "mat.h"

class GlTransform {
    
    private:
        Mat4<float> rotationMatrix;
        
    public:
        Vec3<float> position, scale;
        
        constexpr GlTransform(Vec3<float> position = Vec3<float>::zero, Vec3<float> scale = Vec3<float>::one):
            rotationMatrix(Mat4<float>::Identity),
            position(position),
            scale(scale) {}
        
        inline Mat4<float> RotationMatrix() { return rotationMatrix; }
        
        //TODO: store rotation in quternion so we can lerp it & save space & speed up rotations
        //      Note: will come at the cost of coverting quaternion to matrix
        
        //Note: returns T*R*S where T=translationMatrix, R=rotationMatrix, S=scaleMatrix
        inline Mat4<float> Matrix() {

            Mat4<float> result;
            
            //compute T*R*S - TODO: make sure simd makes 0-value w component in rotation matrix multiply for free
            result.column[0] = rotationMatrix.column[0] * scale.x;
            result.column[1] = rotationMatrix.column[1] * scale.y;
            result.column[2] = rotationMatrix.column[2] * scale.z;
            result.column[3] = Vec4(position.x, position.y, position.z, 1.f);
            
            return result;
        }
        
        inline GlTransform& Scale(const Vec3<float>& s)         { scale+= s; return *this; }
        inline GlTransform& Rotate(const Vec3<float> theta)     { rotationMatrix.Rotate(theta); return *this; }
        inline GlTransform& Translate(const Vec3<float>& delta) { position+= delta; return *this; }
        
};