#pragma once

#include "mat.h"
#include "vec.h"

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
        /** Note: using
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