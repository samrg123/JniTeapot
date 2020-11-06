#pragma once

#include "mat.h"
#include "Quaternion.h"

class GlTransform {
        Quaternion<float> rotation;
    
    public:
        Vec3<float> position, scale;
        
        constexpr GlTransform(Vec3<float> position = Vec3<float>::zero,
                              Vec3<float> scale = Vec3<float>::one,
                              Vec3<float> theta = Vec3<float>::zero):
            rotation(theta),
            position(position),
            scale(scale) {}
        
        inline Quaternion <float> GetRotation() const { return rotation; }
        inline void SetRotation(const Quaternion<float>& r) {
        
            //TODO: if quaternions are prone to decaying then we should just normalize them each time!
            RUNTIME_ASSERT(Approx(r.NormSquared(), 1.f, .01f),
                           "rotation is not unit quaternion { x: %f, y: %f, z: %f, w: %f | Norm: %f } ",
                           r.x, r.y, r.z, r.w, r.Norm());
            
            rotation = r;
        }
        
        //Note: returns S'*R'*T' where T'=inverseTranslationMatrix, R'=inverseRotationMatrix, S'=inverseScaleMatrix
        inline Mat4<float> InverseMatrix() const {
            Vec3<float> inverseTranslation = -position;
            Vec3<float> inverseScale = scale.Inverse();
            
            Mat4<float> result = rotation.Conjugate().Matrix();

            result.column[0]*= inverseScale;
            result.column[1]*= inverseScale;
            result.column[2]*= inverseScale;
            result.column[3] = Vec4(inverseScale.x * result.Row1().Dot(inverseTranslation),
                                    inverseScale.y * result.Row2().Dot(inverseTranslation),
                                    inverseScale.z * result.Row3().Dot(inverseTranslation),
                                    1.f);
            return result;
        }
        
        //Note: returns T*R*S where T=translationMatrix, R=rotationMatrix, S=scaleMatrix
        inline Mat4<float> Matrix() const {
            
            // TODO: make sure simd makes 0-value w component in rotation matrix multiply for free and
            //       rotation.Matrix() operations get inlined
            Mat4<float> result = rotation.Matrix();
            result.column[0]*= scale.x;
            result.column[1]*= scale.y;
            result.column[2]*= scale.z;
            result.column[3] = Vec4(position.x, position.y, position.z, 1.f);
            
            return result;
        }
        
        inline GlTransform& Scale(const Vec3<float>& s)         { scale+= s; return *this; }
        inline GlTransform& Rotate(const Vec3<float> theta)     { rotation.Rotate(theta); return *this; }
        inline GlTransform& Translate(const Vec3<float>& delta) { position+= delta; return *this; }
};