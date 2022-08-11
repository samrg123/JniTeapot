#pragma once

#include "mat.h"
#include "vec.h"
#include "mathUtil.h"

//TODO: TEST THIS!

template <typename T, typename Base = Vec4<T>>
struct Quaternion: Base {
    
    using Base::Base;
    using Base::x, Base::y, Base::z, Base::w;
    using Base::component;
    using Base::NormSquared;
    
    inline Vec3<T> Im() { return (Vec3<T>&)*this; }
    inline float   Re() { return Base::w; }
    
    constexpr Quaternion Conjugate() const { return Quaternion(-x, -y, -z, w); }
    constexpr Quaternion Inverse() const { return Conjugate()/=NormSquared(); }
    
    constexpr Quaternion() = default;
    constexpr Quaternion(const Base& b): Base(b) {}
    
    //Note rotates matrix in theta.z, theta.y, theta.x order
    inline void SetRotation(Vec3<float> theta) {
        theta*= .5f;
        Vec3 ct(FastCos(theta.x), FastCos(theta.y), FastCos(theta.z)),
             st(FastSin(theta.x), FastSin(theta.y), FastSin(theta.z));

        //TODO: make sure repeated 'ct.x * ct.y' and 'st.y * st.z' gets consumed by simd
        x = st.x * ct.y * ct.z - ct.x * st.y * st.z;
        y = ct.x * st.y * ct.z + st.x * ct.y * st.z;
        z = ct.x * ct.y * st.z - st.x * st.y * ct.z;
        w = ct.x * ct.y * ct.z + st.x * st.y * st.z;
    }
    
    constexpr Quaternion(Vec3<float> theta) { SetRotation(theta); }
    
    constexpr Quaternion(Vec3<T> u, float theta) {
        theta = .5f * theta;
        (Vec3<T>&)component = u * FastSin(theta);
        w = FastCos(theta);
    }
    
    template<typename T2> constexpr Quaternion& operator*= (const Quaternion<T2>& q) {
        /** Note: using p*q =
              r.x = p.w*q.x + p.x*q.w + p.y*q.z − p.z*q.y
              r.y = p.w*q.y − p.x*q.z + p.y*q.w + p.z*q.x
              r.z = p.w*q.z + p.x*q.y − p.y*q.x + p.z*q.w
              r.w = p.w*q.w − p.x*q.x - p.y*q.y - p.z*q.z
        **/
        
        //TODO: make sure this simds
    
        //get Vec4 representations
        Vec4<T> colX( q.w, -q.z,  q.y, -q.x),
                colY( q.z,  q.w, -q.x, -q.y),
                colZ(-q.y,  q.x,  q.w, -q.z);
    
        colX*= x;
        colY*= y;
        colZ*= z;
        
        Vec4<T> &r4 = (Vec4<T>&)*this;
        r4 = (const Vec4<T2>&)q * w;
        r4+= colX;
        r4+= colY;
        r4+= colZ;
        
        return *this;
    }
    
    template<typename T2>
    constexpr auto operator*(const Quaternion<T2>& q) const { return Quaternion<decltype(x*q.x)>(*this)*= q; }
    
    constexpr Quaternion& Rotate(Vec3<float> theta) { return  (*this)*= Quaternion(theta); }
    
    template<typename T2>
    constexpr auto ApplyRotation(Vec3<T2> v) {
    
        /** Note: uses r = p * v * p.Inverse() = p * v * (p.Conjugate()/1)
            
            r.x = 	(q.x*v.x + q.y*v.y + q.z*v.z)*q.x +
                    (q.w*v.x + q.y*v.z − q.z*v.y)*q.w +
                   -(q.w*v.y + q.z*v.x − q.x*v.z)*q.z +
                    (q.w*v.z + q.x*v.y − q.y*v.x)*q.y
            
            r.y =   (q.x*v.x + q.y*v.y + q.z*v.z)*q.y +
                    (q.w*v.x + q.y*v.z − q.z*v.y)*q.z +
                    (q.w*v.y + q.z*v.x − q.x*v.z)*q.w +
                   -(q.w*v.z + q.x*v.y − q.y*v.x)*q.x
            
            r.z = 	(q.x*v.x + q.y*v.y + q.z*v.z)*q.z +
                   -(q.w*v.x + q.y*v.z − q.z*v.y)*q.y +
                    (q.w*v.y + q.z*v.x − q.x*v.z)*q.x +
                    (q.w*v.z + q.x*v.y − q.y*v.x)*q.w
            
            r.w = 0
        **/
 
        using Tr = decltype(x*v.x);

        //Note: SANITY CHECK CODE FROM FROM STACK OVERFLOW!!
        Vec3<float> u(x, y, z);
        float s = w;
        return (u * (2.0f * u.Dot(v))) + (v * (s*s - u.Dot(u))) + (u.Cross(v) * (2.0f * s));

        //TODO: make sure this simds
        //TODO: see if this can be optimized a little more
        Tr  v1 = x*v.x + y*v.y + z*v.z,
            v2 = w*v.x + y*v.z - z*v.y,
            v3 = w*v.y + z*v.x - x*v.z,
            v4 = w*v.z + x*v.y - y*v.x;
            
        return Vec3<Tr>(v1*x + v2*w - v3*z + v4*y,
                        v1*y + v2*z + v3*w - v4*x,
                        v1*z - v2*y + v3*x + v4*w);
    }

    template<typename T2>
    inline static Quaternion RotateTo(Vec3<T2> source, Vec3<T2> destination) {

        RUNTIME_ASSERT(Approx(source.NormSquared(), T2(1)), 
                      "source is not normailzed [ source.NormSquared() = %f ]", 
                      float(source.NormSquared()));

        RUNTIME_ASSERT(Approx(destination.NormSquared(), T2(1)), 
                      "destination is not normailzed [ destination.NormSquared() = %f ]", 
                      float(destination.NormSquared()));

        T2 cosTheta = source.Dot(destination);
    
        //check to see if we rotated ~180 degrees
        //Note: Even small epsilon (.0001f) yields visable error in rotation
        //Todo: Make sure that setting epsilon == 0 doesn't introduce errors with nSinTheta
        constexpr float epsilon = 0.f;
        if(cosTheta <= (-1.f + epsilon)) {

            //Try to using crossing destination with x-axis to get axis of rotation
            Vec3 perpendicular = Vec3<T2>::right.Cross(destination);
            
            //Note: x component will be zero after crossing x-axis with destination
            T2 perpendicularNormSquared = Vec2<T2>(perpendicular.y, perpendicular.z).NormSquared();

            //check if destination was co-planer with x axis.
            //If it is use y-axis for rotation, otherwise use perpendicular axis for rotation
            Vec3 rotationNormal = perpendicularNormSquared <= epsilon ?
                                  Vec3<T2>::up :
                                  perpendicular/Sqrt(perpendicularNormSquared);

            //Build quaternion represention rotation
            //Note: theta == Pi, cos(theta/2) = 0, sin(theta/2) = 1,
            //      Q = <sin(theta/2)*n, cos(theta/2)>      | Where n is the axis of rotation
            //      Q = <1*n, 0>
            //      Q = <n, 0>
            return Base(rotationNormal);
        }
    
        Vec3 nSinTheta = source.Cross(destination);
        
        //Note: nSinTheta = sin(theta)*n            | Where n is the axis of rotation
        //      Q  = <n*sin(theta/2), cos(theta/2)> | Where Q is the desired quaternion
        //      Q2 = <n*sin(theta),   cos(theta)>   | Where Q2 represents the a quaternion rotation by 2*theta
        //      Q  = normalize(lerp(Qi, Q2, .5))    | Where Qi is the identity quaternion  
        //      Q  = normalize(.5*(Q2 - Qi) + Qi)
        //      Q  = normalize(.5*Q2 + .5*Qi)
        //      Q  = normalize(Q2 + Qi)
        //      Q  = normalize(<n*sin(theta),  cos(theta)> + <0,0,0,1>)
        //      Q  = normalize(<nSinTheta, cos(theta) + 1>)
        //
        //Note: If we don't need want to require source and destination to be normalized
        //      nSinTheta = norm(source)*norm(destination)*sin(theta)*n, cosTheta = norm(source)*norm(destination)*cos(theta)
        //      and the resulting quaternion simplifies to: 
        //      Base(nSinTheta.x, nSinTheta.y, nSinTheta.z, cosTheta + Sqrt(source.NormSquared() + destination.NormSquared())).Normalize();
        Vec4 q = Base(nSinTheta.x, nSinTheta.y, nSinTheta.z, cosTheta + 1).Normalize();
        
        //Note: Sanity check that q never diverges to nan or infinity
        RUNTIME_ASSERT(Approx(q.NormSquared(), T2(1)), 
                       "Q is not normalized [ q = (%f, %f, %f, %f) | q.NormSquared() = %f ]",
                       float(q.x), float(q.y), float(q.z), float(q.w),  
                       float(q.NormSquared()));
        
        return q;
    }
    
    inline static Quaternion LookAt(Vec3<float> origin, Vec3<float> target, Vec3<float> eyeDirection) {
        return RotateTo(eyeDirection, (target-origin).Normalize());
    }
    
    inline static Quaternion LookAt(Vec3<float> origin, Vec3<float> target) {
        return RotateTo(Vec3<float>::out, (target-origin).Normalize());
    }
    
    //Warn: this assumes quaternion is of unit length
    inline Mat4<T> Matrix() const {
        T x2 = x*2, y2 = y*2, z2 = z*2;
    
        T wx2 = w*x2, wy2 = w*y2, wz2 = w*z2;
        T xx2 = x*x2, xy2 = x*y2, xz2 = x*z2;
        T yy2 = y*y2, yz2 = y*z2;
        T zz2 = z*z2;

        return Mat4<T>({
           1-(yy2 + zz2),     (xy2 + wz2),     (xz2 - wy2), 0,
             (xy2 - wz2),   1-(xx2 + zz2),     (yz2 + wx2), 0,
             (xz2 + wy2),     (yz2 - wx2),   1-(xx2 + yy2), 0,
           0,               0,               0,             1
        });
    }
    
    static inline const Quaternion zero     = Quaternion(0, 0, 0, 0);
    static inline const Quaternion identity = Quaternion(0, 0, 0, 1);
};

//TODO: implement and test these
//vec4 qLerp(vec4 q1, vec4 q2, float t) { return normalize(((1. - t)*q1) + (t*q2)); }
//
//vec4 qSlerp(vec4 q1, vec4 q2, float t) {
//
//    // Note: if q1 and q2 are 90+c > 90 degrees apart from each other
//    //       we can get a shorter arc that represents the same
//    //       rotation by using -q1 and q2 which are 90-c < 90 degrees apart
//    // Note: q * v * q^-1 = (s*q) * v * (s*q)^-1
//    float cosTheta = dot(q1, q2);
//    if(cosTheta < 0.) {
//        q1 = -q1;
//        cosTheta = -cosTheta;
//    }
//
//    // q1 and q2 are ~colinear and acos is unstable so just lerp instead
//    float epsilon = 0.001;
//    if(cosTheta >= (1. - epsilon)) return qLerp(q1, q2, t);
//
//    float theta = acos(cosTheta);
//    float inverseSinTheta = inversesqrt(1. - (cosTheta*cosTheta));
//
//    return ( (sin((1. - t)*theta)*q1) + (sin(t*theta)*q2) ) * inverseSinTheta;
//}