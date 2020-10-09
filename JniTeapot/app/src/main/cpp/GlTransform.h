#pragma once
#include "mat.h"

class GlTransform {
    
    private:
        Mat4<float> rotationTranslationMatrix;
        Vec3<float> scale;
        
        
    public:
        
        constexpr GlTransform(Vec3<float> position = Vec3<float>::zero, Vec3<float> scale = Vec3<float>::one):
            rotationTranslationMatrix({1,          0,          0,          0,
                                       0,          1,          0,          0,
                                       0,          0,          1,          0,
                                       position.x, position.y, position.z, 1}),
            scale(scale) {}
        
        inline void SetPosition(const Vec3<float>& position) { rotationTranslationMatrix.column[3] = position; }
        inline Vec3<float> GetPosition() const { return rotationTranslationMatrix.column[3]; }
        
        inline void SetScale(const Vec3<float>& s) { scale = s; }
        inline Vec3<float> GetScale() const { return scale; }
        
        inline Mat4<float> Matrix() {
            Mat4<float> tMatrix = rotationTranslationMatrix;
            return tMatrix.Scale(scale);
        }
        
        //TODO: we don't track rotation yet - TODO: consider storing rotation in quternion so we can lerp it & save space & speed up rotations
        //      Note: will come at the cost of coverting quaternion to matrix
        inline GlTransform& Rotate(const Vec3<float> theta) {
            rotationTranslationMatrix.Rotate(theta);
            return *this;
        }
        
        inline GlTransform& Translate(const Vec3<float>& delta) {
            rotationTranslationMatrix.column[3]+= delta;
            return *this;
        }
        
};