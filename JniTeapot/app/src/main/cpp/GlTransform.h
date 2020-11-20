#pragma once

#include "mat.h"
#include "Quaternion.h"

class GlTransform {
        Quaternion<float> rotation;
    
    public:
        Vec3<float> position, scale;
        
        
        inline Quaternion <float> GetRotation() const { return rotation; }
        inline void SetRotation(const Quaternion<float>& r) {
            
            //TODO: if quaternions are prone to decaying then we should just normalize them each time!
            RUNTIME_ASSERT(Approx(r.NormSquared(), 1.f, .01f),
                           "rotation is not unit quaternion { x: %f, y: %f, z: %f, w: %f | Norm: %f } ",
                           r.x, r.y, r.z, r.w, r.Norm());
            
            rotation = r;
        }
        
        constexpr GlTransform(Vec3<float> position = Vec3<float>::zero,
                              Vec3<float> scale = Vec3<float>::one,
                              Quaternion<float> rotation = Quaternion<float>::identity): position(position),
                                                                                         scale(scale) {
            SetRotation(rotation);
        }
        
        constexpr GlTransform(Vec3<float> position, Vec3<float> scale, Vec3<float> theta): rotation(theta),
                                                                                           position(position),
                                                                                           scale(scale) {}
        
        
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
        
        // returns a matrix such that normal vector n = (x, y, z 0) will be oriented in right direction
        // n' = (NormalMatrix()*n).xyz = (x', y', z', 0) where ...
        inline Mat4<float> NormalMatrix() const {

            //Note: this returns the equivalent to Inverse().Transpose() without translations
            //Note: Inverse().Transpose = ((T*R*S)^-1)^T = (T^-1 * R^-1 * S^-1)^T = (S^-1 * R * T^(-1T))
            //      Because normal vectors are translation invarient n.w = 0 and we can simplify
            //      the normal Matrix to: (S^-1 * R) [Note: T^(-1T) = [I | [-x, -y, -z, 1]], and R is a 3x3 matrix]
            
            //Note: inverse rotationMatrix is orthogonal so R^-1 = R^t and (R^-1)^t = R
            Vec3 inverseScale = scale.Inverse();
            
            Mat4<float> result = rotation.Matrix();
            result.column[0]*= inverseScale.x;
            result.column[1]*= inverseScale.y;
            result.column[2]*= inverseScale.z;
    
            ////Note: this would be the math used to include translations
            //Vec3 inversePosition = -position;
            //result.d1 = result.column[0].Dot(inversePosition);
            //result.d2 = result.column[1].Dot(inversePosition);
            //result.d3 = result.column[2].Dot(inversePosition);
            
            return result;
        }
        
        inline GlTransform& Scale(const Vec3<float>& s)         { scale+= s; return *this; }
        inline GlTransform& Rotate(const Vec3<float> theta)     { rotation.Rotate(theta); return *this; }
        inline GlTransform& Translate(const Vec3<float>& delta) { position+= delta; return *this; }
};