#pragma once

#include "mat.h"
#include "Quaternion.h"

class GlTransform {
    public:
        Quaternion<float> rotation;
        Vec3<float> position, scale;
        
        constexpr GlTransform(Vec3<float> position = Vec3<float>::zero,
                              Vec3<float> scale = Vec3<float>::one,
                              Vec3<float> theta = Vec3<float>::zero):
            rotation(theta),
            position(position),
            scale(scale) {}
        
        inline Mat4<float> RotationMatrix() { return rotation.Matrix(); }
        
        //Note: returns T*R*S where T=translationMatrix, R=rotationMatrix, S=scaleMatrix
        inline Mat4<float> Matrix() {
    
            //TODO: if quaternions are prone to decaying then we should just normalize them each time!
            RUNTIME_ASSERT(Approx(rotation.NormSquared(), 1.f, .01f),
                           "rotation is not unit quaternion { x: %f, y: %f, z: %f, w: %f | Norm: %f } ",
                           rotation.x, rotation.y, rotation.z, rotation.w, rotation.Norm());
            
            //compute T*R*S

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