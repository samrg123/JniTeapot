#pragma once

#include "vec.h"
#include "mathUtil.h"

template<typename T>
struct Mat4 {
    
    //Note: column major to match opengl
    union {
        struct {
            T a1, b1, c1, d1,
              a2, b2, c2, d2,
              a3, b3, c3, d3,
              a4, b4, c4, d4;
        };
        
        Vec4<float> column[4];
        T components[4][4];
        T values[16];
    };
    
    constexpr Mat4() = default;
    
    //TODO: replace this with 4xsimd move
    constexpr Mat4(const T (&v)[16] ): components{ {v[0],  v[1],  v[2],  v[3]},
                                                   {v[4],  v[5],  v[6],  v[7]},
                                                   {v[8],  v[9],  v[10], v[11]},
                                                   {v[12], v[13], v[14], v[15]}} {}
    
    constexpr Mat4(const T* v):        components{ {v[0],  v[1],  v[2],  v[3]},
                                                   {v[4],  v[5],  v[6],  v[7]},
                                                   {v[8],  v[9],  v[10], v[11]},
                                                   {v[12], v[13], v[14], v[15]}} {}
    
    //TODO: make sure this SIMD swizzles
    inline Vec4<T> Row1() { return Vec4(a1, a2, a3, a4); }
    inline Vec4<T> Row2() { return Vec4(b1, b2, b3, b4); }
    inline Vec4<T> Row3() { return Vec4(c1, c2, c3, c4); }
    inline Vec4<T> Row4() { return Vec4(d1, d2, d3, d4); }
    
    template<typename T2>
    constexpr Mat4& operator*= (const Mat4<T2>& m) {
        
        Vec4 row1 = Row1(),
             row2 = Row2(),
             row3 = Row3(),
             row4 = Row4();
        
        Vec4 col = m.column[0];
        a1 = row1.Dot(col);
        b1 = row2.Dot(col);
        c1 = row3.Dot(col);
        d1 = row4.Dot(col);
    
        col = m.column[1];
        a2 = row1.Dot(col);
        b2 = row2.Dot(col);
        c2 = row3.Dot(col);
        d2 = row4.Dot(col);
    
        col = m.column[2];
        a3 = row1.Dot(col);
        b3 = row2.Dot(col);
        c3 = row3.Dot(col);
        d3 = row4.Dot(col);
    
        col = m.column[3];
        a4 = row1.Dot(col);
        b4 = row2.Dot(col);
        c4 = row3.Dot(col);
        d4 = row4.Dot(col);
        
        return *this;
    }

    template<typename T2>
    constexpr auto operator* (const Mat4<T2>& m) {
        Mat4<decltype(a1*m.a1)> result(*this);
        return result*= m;
    }
    
    constexpr Mat4 Transpose() const {
        return Mat4({a1, a2, a3, a4,
                     b1, b2, b3, b4,
                     c1, c2, c3, c4,
                     d1, d2, d3, d4});
    }
    
    //Note: this code was borrowed from the mesa library
    //TODO: read through this and clean it up a bit
    constexpr Mat4 Inverse() const {

        Mat4 inverse;

        const T* m = values;
        T* inv = inverse.values;
        
        inv[0] =   m[5]  * m[10] * m[15] -
                   m[5]  * m[11] * m[14] -
                   m[9]  * m[6]  * m[15] +
                   m[9]  * m[7]  * m[14] +
                   m[13] * m[6]  * m[11] -
                   m[13] * m[7]  * m[10];
    
        inv[4] =  -m[4]  * m[10] * m[15] +
                   m[4]  * m[11] * m[14] +
                   m[8]  * m[6]  * m[15] -
                   m[8]  * m[7]  * m[14] -
                   m[12] * m[6]  * m[11] +
                   m[12] * m[7]  * m[10];
    
        inv[8] =   m[4]  * m[9] * m[15] -
                   m[4]  * m[11] * m[13] -
                   m[8]  * m[5] * m[15] +
                   m[8]  * m[7] * m[13] +
                   m[12] * m[5] * m[11] -
                   m[12] * m[7] * m[9];
    
        inv[12] = -m[4]  * m[9] * m[14] +
                   m[4]  * m[10] * m[13] +
                   m[8]  * m[5] * m[14] -
                   m[8]  * m[6] * m[13] -
                   m[12] * m[5] * m[10] +
                   m[12] * m[6] * m[9];
    
        inv[1] =  -m[1]  * m[10] * m[15] +
                   m[1]  * m[11] * m[14] +
                   m[9]  * m[2] * m[15] -
                   m[9]  * m[3] * m[14] -
                   m[13] * m[2] * m[11] +
                   m[13] * m[3] * m[10];
    
        inv[5] =   m[0]  * m[10] * m[15] -
                   m[0]  * m[11] * m[14] -
                   m[8]  * m[2] * m[15] +
                   m[8]  * m[3] * m[14] +
                   m[12] * m[2] * m[11] -
                   m[12] * m[3] * m[10];
    
        inv[9] =  -m[0]  * m[9] * m[15] +
                   m[0]  * m[11] * m[13] +
                   m[8]  * m[1] * m[15] -
                   m[8]  * m[3] * m[13] -
                   m[12] * m[1] * m[11] +
                   m[12] * m[3] * m[9];
    
        inv[13] =  m[0]  * m[9] * m[14] -
                   m[0]  * m[10] * m[13] -
                   m[8]  * m[1] * m[14] +
                   m[8]  * m[2] * m[13] +
                   m[12] * m[1] * m[10] -
                   m[12] * m[2] * m[9];
    
        inv[2] =   m[1]  * m[6] * m[15] -
                   m[1]  * m[7] * m[14] -
                   m[5]  * m[2] * m[15] +
                   m[5]  * m[3] * m[14] +
                   m[13] * m[2] * m[7] -
                   m[13] * m[3] * m[6];
    
        inv[6] =  -m[0]  * m[6] * m[15] +
                   m[0]  * m[7] * m[14] +
                   m[4]  * m[2] * m[15] -
                   m[4]  * m[3] * m[14] -
                   m[12] * m[2] * m[7] +
                   m[12] * m[3] * m[6];
    
        inv[10] =  m[0]  * m[5] * m[15] -
                   m[0]  * m[7] * m[13] -
                   m[4]  * m[1] * m[15] +
                   m[4]  * m[3] * m[13] +
                   m[12] * m[1] * m[7] -
                   m[12] * m[3] * m[5];
    
        inv[14] = -m[0]  * m[5] * m[14] +
                   m[0]  * m[6] * m[13] +
                   m[4]  * m[1] * m[14] -
                   m[4]  * m[2] * m[13] -
                   m[12] * m[1] * m[6] +
                   m[12] * m[2] * m[5];
    
        inv[3] =  -m[1] * m[6] * m[11] +
                   m[1] * m[7] * m[10] +
                   m[5] * m[2] * m[11] -
                   m[5] * m[3] * m[10] -
                   m[9] * m[2] * m[7] +
                   m[9] * m[3] * m[6];
    
        inv[7] =   m[0] * m[6] * m[11] -
                   m[0] * m[7] * m[10] -
                   m[4] * m[2] * m[11] +
                   m[4] * m[3] * m[10] +
                   m[8] * m[2] * m[7] -
                   m[8] * m[3] * m[6];
    
        inv[11] = -m[0] * m[5] * m[11] +
                   m[0] * m[7] * m[9] +
                   m[4] * m[1] * m[11] -
                   m[4] * m[3] * m[9] -
                   m[8] * m[1] * m[7] +
                   m[8] * m[3] * m[5];
    
        inv[15] =  m[0] * m[5] * m[10] -
                   m[0] * m[6] * m[9] -
                   m[4] * m[1] * m[10] +
                   m[4] * m[2] * m[9] +
                   m[8] * m[1] * m[6] -
                   m[8] * m[2] * m[5];
    
        //Note: computing the determinate after save 68 multiplications
        float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        if(det == 0.) return zero;
        det = 1.0 / det;
    
        inverse.column[0]*= det;
        inverse.column[1]*= det;
        inverse.column[2]*= det;
        inverse.column[3]*= det;
        
        return inverse;
    }

    static inline Mat4 identity = Mat4({1, 0, 0, 0,
                                        0, 1, 0, 0,
                                        0, 0, 1, 0,
                                        0, 0, 0, 1});
    
    static inline Mat4 zero = Mat4({0, 0, 0, 0,
                                    0, 0, 0, 0,
                                    0, 0, 0, 0,
                                    0, 0, 0, 0});
    
    static inline Mat4 one = Mat4({1, 1, 1, 1,
                                   1, 1, 1, 1,
                                   1, 1, 1, 1,
                                   1, 1, 1, 1});
    
    static
    inline Mat4 Orthogonal(const Vec2<float>& view, float nearPlane, float farPlane) {
        
        //Note: this is right handed use '-negInvPlaneDelta, -zOffset for left handed'
        float negInvPlaneDelta = 1.f/(nearPlane - farPlane);
        float zOffset = (farPlane + nearPlane)*negInvPlaneDelta;
        
        return Mat4({  2.f/view.x, 0,          0,                  0,
                       0,          2.f/view.y, 0,                  0,
                       0,          0,          2*negInvPlaneDelta, 0,
                       0,          0,          zOffset,            1
                    });
    }
    
    static
    inline Mat4 Perspective(float aspect, float fovX, float nearPlane, float farPlane) {
        
        float negTanX = -FastTan(.5f*fovX);
        
        float zScaler, zOffset;
        float planeDelta = farPlane - nearPlane;
        
        //TODO: Think of a way to handle this.. somhow have to clip z 
        //      values that are not co-planer with near plane. Should 
        //      we just use small epsilon for plane delta instead? 
        RUNTIME_ASSERT(planeDelta != 0, "planeDelta is zero [nearPlane: %f, farPlane: %f]", nearPlane, farPlane);

        float invPlaneDelta = 1.f/planeDelta;
        zScaler = negTanX*farPlane*invPlaneDelta;
        zOffset = nearPlane*zScaler;

        //Note: this uses right handed coordinates
        return Mat4({  1,     0,      0,        0,
                       0,     aspect, 0,        0,
                       0,     0,      zScaler,  negTanX,
                       0,     0,      zOffset,  0
                    });
    }
    
    //Note rotates matrix representing 3D transform in theta.z, theta.y, theta.x order
    inline Mat4& Rotate(const Vec3<float>& theta) {
    
        //TODO: make sure this SIMD sin/cos
        Vec3 cosTheta(FastCos(theta.x), FastCos(theta.y), FastCos(theta.z)),
             sinTheta(FastSin(theta.x), FastSin(theta.y), FastSin(theta.z));
        
        //TODO: see if we can simplify math more
        float cosThetaXSinThetaY = cosTheta.x*sinTheta.y,
              sinThetaXSinThetaY = sinTheta.x*sinTheta.y;
    
        Vec3 row1(cosTheta.x*cosTheta.y,
                  cosThetaXSinThetaY*sinTheta.z - sinTheta.x*cosTheta.z,
                  cosThetaXSinThetaY*cosTheta.z + sinTheta.x*sinTheta.z);
    
        Vec3 row2(sinTheta.x*cosTheta.y,
                  sinThetaXSinThetaY*sinTheta.z + cosTheta.x*cosTheta.z,
                  sinThetaXSinThetaY*cosTheta.z - cosTheta.x*sinTheta.z);
    
        Vec3 row3(-sinTheta.y,
                  cosTheta.y*sinTheta.z,
                  cosTheta.y*cosTheta.z);
    
        Vec3 col = (Vec3<float>)column[0];
        a1 = row1.Dot(col);
        b1 = row2.Dot(col);
        c1 = row3.Dot(col);
    
        col = (Vec3<float>)column[1];
        a2 = row1.Dot(col);
        b2 = row2.Dot(col);
        c2 = row3.Dot(col);
    
        col = (Vec3<float>)column[2];
        a3 = row1.Dot(col);
        b3 = row2.Dot(col);
        c3 = row3.Dot(col);
    
        col = (Vec3<float>)column[3];
        a4 = row1.Dot(col);
        b4 = row2.Dot(col);
        c4 = row3.Dot(col);

        return *this;
    }
    
    //Note scales matrix representing 3D transform by s
    inline Mat4& Scale(const Vec3<float>& s) {
        column[0]*= s;
        column[1]*= s;
        column[2]*= s;
        return *this;
    }

    //Note translates matrix representing 3D transform by s
    inline Mat4& Translate(const Vec3<float>& delta) {
        column[3]+= delta;
        return *this;
    }
};