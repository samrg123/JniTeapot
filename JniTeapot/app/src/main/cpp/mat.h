#pragma once

#include "util.h"
#include "vec.h"

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
    };
    
    constexpr Mat4(){}
    
    //TODO: replace this with 4xsimd move?
    constexpr Mat4(const T (&v)[16] ): components{ {v[0],  v[1],  v[2],  v[3]},
                                                   {v[4],  v[5],  v[6],  v[7]},
                                                   {v[8],  v[9],  v[10], v[11]},
                                                   {v[12], v[13], v[14], v[15]}} {}

    template<typename T2>
    constexpr Mat4& operator*= (const Mat4<T2>& m) {
        
        //TODO: make sure this SIMD swizzles
        Vec4 row1(a1, a2, a3, a4),
             row2(b1, b2, b3, b4),
             row3(c1, c2, c3, c4),
             row4(d1, d2, d3, d4);
        
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

    static inline Mat4 Identity = Mat4({1, 0, 0, 0,
                                        0, 1, 0, 0,
                                        0, 0, 1, 0,
                                        0, 0, 0, 1});
    
    static inline Mat4 Zero = Mat4({});
    static inline Mat4 One = Mat4({1, 1, 1, 1,
                                   1, 1, 1, 1,
                                   1, 1, 1, 1,
                                   1, 1, 1, 1});
    
    static
    inline Mat4 Orthogonal(const Vec2<float>& view, float nearPlane, float farPlane) {
        
        float iD = 1.f/(farPlane - nearPlane),
            oD = -iD*(nearPlane + farPlane);
        
        return Mat4({  2.f/view.x, 0,          0,    0,
                       0,          2.f/view.y, 0,    0,
                       0,          0,          2*iD, 0,
                       0,          0,          oD,   1
                    });
    }
    
    static
    inline Mat4 Perspective(float aspect, float fovX, float nearPlane, float farPlane) {
        
        float thetaX = .5f*fovX,
              iTanX  = 1.f/FastTan(thetaX),
              iTanY  = aspect * iTanX;
        
        float iD = 1.f/(farPlane - nearPlane),
              sD = iD*(nearPlane + farPlane),
              oD = -2.f*farPlane*nearPlane*iD;
        
        return Mat4({  iTanX, 0,     0,   0,
                       0,     iTanY, 0,   0,
                       0,     0,     sD,  1,
                       0,     0,     oD,  0
                    });
    }
    
    //rotation matrix rotates in thata.z, theta.y, theta.x order
    inline Mat4& Rotate(const Vec3<float>& theta) {
    
        //TODO: make sure this SIMD sin/cos
        Vec3 cosTheta(FastCos(theta.x), FastCos(theta.y), FastCos(theta.z)),
             sinTheta(FastSin(theta.x), FastSin(theta.y), FastSin(theta.z));
        
        //TODO: see if we can simplify math more
        float cosThetaXSinThetaY = cosTheta.x*sinTheta.y,
              sinThetaXSinThetaY = sinTheta.x*sinTheta.y;

        Vec3 col1(cosTheta.x*cosTheta.y,
                  sinTheta.x*cosTheta.y,
                  -sinTheta.y);
        
        Vec3 col2(cosThetaXSinThetaY*sinTheta.z - sinTheta.x*cosTheta.z,
                  sinThetaXSinThetaY*sinTheta.z + cosTheta.x*cosTheta.z,
                  cosTheta.y*sinTheta.z);
        
        Vec3 col3(cosThetaXSinThetaY*cosTheta.z + sinTheta.x*sinTheta.z,
                  sinThetaXSinThetaY*cosTheta.z - cosTheta.x*sinTheta.z,
                  cosTheta.y*cosTheta.z);
    
        //TODO: make sure this SIMD swizzles
        Vec3 row1(a1, a2, a3),
             row2(b1, b2, b3),
             row3(c1, c2, c3);
  
        a1 = row1.Dot(col1);
        b1 = row2.Dot(col1);
        c1 = row3.Dot(col1);
    
        a2 = row1.Dot(col2);
        b2 = row2.Dot(col2);
        c2 = row3.Dot(col2);
    
        a3 = row1.Dot(col3);
        b3 = row2.Dot(col3);
        c3 = row3.Dot(col3);

        return *this;
    }
    
    inline Mat4& Scale(const Vec3<float>& s) {
        a1*= s.x;
        b2*= s.y;
        c3*= s.z;
        return *this;
    }
};