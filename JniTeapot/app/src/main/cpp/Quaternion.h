#pragma once

#include "vec.h"
#include "util.h"

//TODO: TEST THIS!

template <typename T, typename Base = Vec4<T>>
struct Quaternion: Base {
    
    inline Vec3<T>& ImPart() { return (Vec3<T>&)(Base::component); }
    
    constexpr Quaternion(const Vec3<T>& v): Base(v.x, v.y, v.z, 0) {};
    
    Quaternion(float theta, Vec3<T> u) {
        theta = .5f * theta;
        ImPart() = u * FastSin(theta);
        Base::w = FastCos(theta);
    }
    
    inline Quaternion Inverse() const { return Quaternion(-Base::x, -Base::y, -Base::z, Base::w); }
    
    template<typename T2> constexpr Quaternion operator* (const Quaternion<T2>& q) const {
        
        //TODO: bake this down to 9 multiplications

        return Quaternion(Base::w*q.x + Base::x*q.w + Base::y*q.z - Base::z*q.y,
                          Base::w*q.y - Base::x*q.z + Base::y*q.w + Base::z*q.x,
                          Base::w*q.z + Base::x*q.y - Base::y*q.x + Base::z*q.w,
                          Base::w*q.w - Base::x*q.x - Base::y*q.y - Base::z*q.z);
    }
    
    template<typename T2> constexpr Quaternion& operator*= (const Quaternion<T2>& q) { return (*this = *this*q); }
    template<typename T2> auto Rotate(Vec3<T2> u) {
        
        //TODO: bake this down to optimize away the 0 terms
        //      should look something like u + 2*cross(cross(u, ImPart()) + w*u, ImPart())
        auto result = (*this) * Quaternion<T2>(u) * Inverse();
        return Vec3<decltype(u.x * Base::x)>(result.x, result.y, result.z);
    }
};