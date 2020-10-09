#pragma once

template<auto kNumDims_>
struct Vec {
    static constexpr auto kNumDims = kNumDims_;
};

//Note: not sure if we're going to want to store vectors in a simd/neon registers or something,
//      so I left different dimensions vectors seperate

template <typename T, typename Base = Vec<2>>
struct Vec2 : Base {
    
    union {
        struct { T x, y; };
        T component[Base::kNumDims];
    };
    constexpr T& operator[] (uint8 n) { return component[n]; }

    constexpr Vec2() {}
    constexpr Vec2(const T& x, const T& y): x(x), y(y) {}
 
    static inline const Vec2 up    = Vec2(0, 1);
    static inline const Vec2 down  = Vec2(0, -1);
    static inline const Vec2 left  = Vec2(-1, 0);
    static inline const Vec2 right = Vec2(1, 0);
    static inline const Vec2 one   = Vec2(1, 1);
    static inline const Vec2 zero  = Vec2(0, 0);
    
    constexpr T Area() const { return x*y; }
    constexpr Vec2 Inverse() const { return Vec2(1/x, 1/y); }
    
    template<typename T2> constexpr auto Dot(const Vec2<T2>& v) const { return v.x*x + v.y*y; }
    
    constexpr const T* operator& () const { return component; };
    template<typename T2> constexpr operator Vec2<T2>() const { return Vec2<T2>(T2(x), T2(y)); };
    
    template<typename St> constexpr auto operator*  (const St& s) const { return Vec2<decltype(x*s)>(x*s, y*s); }
    template<typename St> constexpr auto operator/  (const St& s) const { return Vec2<decltype(x/s)>(x/s, y/s); }
    template<typename St> constexpr auto operator+  (const St& s) const { return Vec2<decltype(x+s)>(x+s, y+s); }
    template<typename St> constexpr auto operator-  (const St& s) const { return Vec2<decltype(x-s)>(x-s, y-s); }
    
    template<typename St> constexpr Vec2& operator*= (const St& s) { x*= s; y*= s; return *this; }
    template<typename St> constexpr Vec2& operator/= (const St& s) { x/= s; y/= s; return *this; }
    template<typename St> constexpr Vec2& operator+= (const St& s) { x+= s; y+= s; return *this; }
    template<typename St> constexpr Vec2& operator-= (const St& s) { x-= s; y-= s; return *this; }
    
    template<typename T2> constexpr auto operator* (const Vec2<T2>& v) const { return Vec2<decltype(x*x)>(x*v.x, y*v.y); }
    template<typename T2> constexpr auto operator/ (const Vec2<T2>& v) const { return Vec2<decltype(x/x)>(x/v.x, y/v.y); }
    template<typename T2> constexpr auto operator+ (const Vec2<T2>& v) const { return Vec2<decltype(x+x)>(x+v.x, y+v.y); }
    template<typename T2> constexpr auto operator- (const Vec2<T2>& v) const { return Vec2<decltype(x-x)>(x-v.x, y-v.y); }
    
    template<typename T2> constexpr Vec2& operator*= (const Vec2<T2>& v) { x*= v.x; y*= v.y; return *this; }
    template<typename T2> constexpr Vec2& operator/= (const Vec2<T2>& v) { x/= v.x; y/= v.y; return *this; }
    template<typename T2> constexpr Vec2& operator+= (const Vec2<T2>& v) { x+= v.x; y+= v.y; return *this; }
    template<typename T2> constexpr Vec2& operator-= (const Vec2<T2>& v) { x-= v.x; y-= v.y; return *this; }
};

template <typename T, typename Base = Vec<3>>
struct Vec3 : Base {
    
    union {
        struct { T x, y, z; };
        T component[Base::kNumDims];
    };
    constexpr T& operator[] (uint8 n) { return component[n]; }
    
    constexpr Vec3() {}
    constexpr Vec3(const T& x, const T& y, const T& z): x(x), y(y), z(z) {}
    template<typename T2> constexpr Vec3(const Vec2<T2>& v): x(v.x), y(v.y), z(0) {}
    
    static inline const Vec3 in    = Vec3( 0,  0,  1);
    static inline const Vec3 out   = Vec3( 0,  0, -1);
    static inline const Vec3 up    = Vec3( 0,  1,  0);
    static inline const Vec3 down  = Vec3( 0, -1,  0);
    static inline const Vec3 left  = Vec3(-1,  0,  0);
    static inline const Vec3 right = Vec3( 1,  0,  0);
    static inline const Vec3 one   = Vec3( 1,  1,  1);
    static inline const Vec3 zero  = Vec3( 0,  0,  0);
    
    constexpr T Area() const { return x*y*z; }
    constexpr Vec3 Inverse() const { return Vec3(1/x, 1/y, 1/z); }
    
    template<typename T2> constexpr auto Dot(const Vec3<T2>& v) const { return v.x*x + v.y*y + v.z*z; }
    
    constexpr const T* operator& () const { return component; };
    constexpr operator Vec2<T>() const    { return (Vec2<T>&)*this; };
    constexpr operator Vec2<T>&()         { return (Vec2<T>&)*this; };
    template<typename T2> constexpr operator Vec3<T2>() const { return Vec3<T2>(T2(x), T2(y), T2(z)); };
    
    template<typename St> constexpr auto operator*  (const St& s) const { return Vec3<decltype(x*s)>(x*s, y*s, z*s); }
    template<typename St> constexpr auto operator/  (const St& s) const { return Vec3<decltype(x/s)>(x/s, y/s, z/s); }
    template<typename St> constexpr auto operator+  (const St& s) const { return Vec3<decltype(x+s)>(x+s, y+s, z+s); }
    template<typename St> constexpr auto operator-  (const St& s) const { return Vec3<decltype(x-s)>(x-s, y-s, z-s); }
    
    template<typename St> constexpr Vec3& operator*= (const St& s) { x*= s; y*= s; z*= s; return *this; }
    template<typename St> constexpr Vec3& operator/= (const St& s) { x/= s; y/= s; z/= s; return *this; }
    template<typename St> constexpr Vec3& operator+= (const St& s) { x+= s; y+= s; z+= s; return *this; }
    template<typename St> constexpr Vec3& operator-= (const St& s) { x-= s; y-= s; z-= s; return *this; }
    
    template<typename T2> constexpr auto operator* (const Vec3<T2>& v) const { return Vec3<decltype(x*x)>(x*v.x, y*v.y, z*v.z); }
    template<typename T2> constexpr auto operator/ (const Vec3<T2>& v) const { return Vec3<decltype(x/x)>(x/v.x, y/v.y, z/v.z); }
    template<typename T2> constexpr auto operator+ (const Vec3<T2>& v) const { return Vec3<decltype(x+x)>(x+v.x, y+v.y, z+v.z); }
    template<typename T2> constexpr auto operator- (const Vec3<T2>& v) const { return Vec3<decltype(x-x)>(x-v.x, y-v.y, z-v.z); }
    
    template<typename T2> constexpr Vec3& operator*= (const Vec3<T2>& v) { x*= v.x; y*= v.y; z*= v.z; return *this; }
    template<typename T2> constexpr Vec3& operator/= (const Vec3<T2>& v) { x/= v.x; y/= v.y; z/= v.z; return *this; }
    template<typename T2> constexpr Vec3& operator+= (const Vec3<T2>& v) { x+= v.x; y+= v.y; z+= v.z; return *this; }
    template<typename T2> constexpr Vec3& operator-= (const Vec3<T2>& v) { x-= v.x; y-= v.y; z-= v.z; return *this; }
    
    template<typename T2> constexpr auto operator* (const Vec2<T2>& v) const { return Vec3<decltype(x*x)>(x*v.x, y*v.y, z); }
    template<typename T2> constexpr auto operator/ (const Vec2<T2>& v) const { return Vec3<decltype(x/x)>(x/v.x, y/v.y, z); }
    template<typename T2> constexpr auto operator+ (const Vec2<T2>& v) const { return Vec3<decltype(x+x)>(x+v.x, y+v.y, z); }
    template<typename T2> constexpr auto operator- (const Vec2<T2>& v) const { return Vec3<decltype(x-x)>(x-v.x, y-v.y, z); }
    
    template<typename T2> constexpr Vec3& operator*= (const Vec2<T2>& v) { x*= v.x; y*= v.y; return *this; }
    template<typename T2> constexpr Vec3& operator/= (const Vec2<T2>& v) { x/= v.x; y/= v.y; return *this; }
    template<typename T2> constexpr Vec3& operator+= (const Vec2<T2>& v) { x+= v.x; y+= v.y; return *this; }
    template<typename T2> constexpr Vec3& operator-= (const Vec2<T2>& v) { x-= v.x; y-= v.y; return *this; }
};

template <typename T, typename Base = Vec<4>>
struct Vec4 : Base {
    
    union {
        struct { T x, y, z, w; };
        T component[Base::kNumDims];
    };
    constexpr T& operator[] (uint8 n) { return component[n]; }
    
    constexpr Vec4() {}
    constexpr Vec4(const T& x, const T& y, const T& z, const T& w): x(x), y(y), z(z), w(w) {}
    template<typename T2> constexpr Vec4(const Vec2<T2>& v): x(v.x), y(v.y), z(0),   w(0) {}
    template<typename T2> constexpr Vec4(const Vec3<T2>& v): x(v.x), y(v.y), z(v.z), w(0) {}
    
    static inline const Vec4 in    = Vec4( 0,  0,  1, 0);
    static inline const Vec4 out   = Vec4( 0,  0, -1, 0);
    static inline const Vec4 up    = Vec4( 0,  1,  0, 0);
    static inline const Vec4 down  = Vec4( 0, -1,  0, 0);
    static inline const Vec4 left  = Vec4(-1,  0,  0, 0);
    static inline const Vec4 right = Vec4( 1,  0,  0, 0);
    static inline const Vec4 one   = Vec4( 1,  1,  1, 1);
    static inline const Vec4 zero  = Vec4( 0,  0,  0, 0);
    
    constexpr T Area() const { return x*y*z*w; }
    constexpr Vec4 Inverse() const { return Vec4(1/x, 1/y, 1/z, 1/w); }
    
    template<typename T2> constexpr auto Dot(const Vec4<T2>& v) const { return v.x*x + v.y*y + v.z*z + + v.w*w; }
    
    constexpr const T* operator& () const { return component; };
    constexpr operator Vec3<T>() const    { return (Vec3<T>&)*this; };
    constexpr operator Vec2<T>() const    { return (Vec3<T>&)*this; };
    constexpr operator Vec3<T>&()         { return (Vec3<T>&)*this; };
    constexpr operator Vec2<T>&()         { return (Vec3<T>&)*this; };
    template<typename T2> constexpr operator Vec4<T2>() const { return Vec4<T2>(T2(x), T2(y), T2(z), T2(w)); };
    
    template<typename St> constexpr auto operator*  (const St& s) const { return Vec4<decltype(x*s)>(x*s, y*s, z*s, w*s); }
    template<typename St> constexpr auto operator/  (const St& s) const { return Vec4<decltype(x/s)>(x/s, y/s, z/s, w/s); }
    template<typename St> constexpr auto operator+  (const St& s) const { return Vec4<decltype(x+s)>(x+s, y+s, z+s, w+s); }
    template<typename St> constexpr auto operator-  (const St& s) const { return Vec4<decltype(x-s)>(x-s, y-s, z-s, w-s); }
    
    template<typename St> constexpr Vec4& operator*= (const St& s) { x*= s; y*= s; z*= s; w*= s; return *this; }
    template<typename St> constexpr Vec4& operator/= (const St& s) { x/= s; y/= s; z/= s; w/= s; return *this; }
    template<typename St> constexpr Vec4& operator+= (const St& s) { x+= s; y+= s; z+= s; w+= s; return *this; }
    template<typename St> constexpr Vec4& operator-= (const St& s) { x-= s; y-= s; z-= s; w-= s; return *this; }

    
    template<typename T2> constexpr auto operator* (const Vec4<T2>& v) const { return Vec4<decltype(x*x)>(x*v.x, y*v.y, z*v.z, w*v.w); }
    template<typename T2> constexpr auto operator/ (const Vec4<T2>& v) const { return Vec4<decltype(x/x)>(x/v.x, y/v.y, z/v.z, w/v.w); }
    template<typename T2> constexpr auto operator+ (const Vec4<T2>& v) const { return Vec4<decltype(x+x)>(x+v.x, y+v.y, z+v.z, w+v.w); }
    template<typename T2> constexpr auto operator- (const Vec4<T2>& v) const { return Vec4<decltype(x-x)>(x-v.x, y-v.y, z-v.z, w-v.w); }
    
    template<typename T2> constexpr Vec4& operator*= (const Vec4<T2>& v) { x*= v.x; y*= v.y; z*= v.z; w*= v.w; return *this; }
    template<typename T2> constexpr Vec4& operator/= (const Vec4<T2>& v) { x/= v.x; y/= v.y; z/= v.z; w/= v.w; return *this; }
    template<typename T2> constexpr Vec4& operator+= (const Vec4<T2>& v) { x+= v.x; y+= v.y; z+= v.z; w+= v.w; return *this; }
    template<typename T2> constexpr Vec4& operator-= (const Vec4<T2>& v) { x-= v.x; y-= v.y; z-= v.z; w-= v.w; return *this; }
    
    template<typename T2> constexpr auto operator* (const Vec3<T2>& v) const { return Vec4<decltype(x*x)>(x*v.x, y*v.y, z*v.z, w); }
    template<typename T2> constexpr auto operator/ (const Vec3<T2>& v) const { return Vec4<decltype(x/x)>(x/v.x, y/v.y, z/v.z, w); }
    template<typename T2> constexpr auto operator+ (const Vec3<T2>& v) const { return Vec4<decltype(x+x)>(x+v.x, y+v.y, z+v.z, w); }
    template<typename T2> constexpr auto operator- (const Vec3<T2>& v) const { return Vec4<decltype(x-x)>(x-v.x, y-v.y, z-v.z, w); }
    
    template<typename T2> constexpr Vec4& operator*= (const Vec3<T2>& v) { x*= v.x; y*= v.y; z*= v.z; return *this; }
    template<typename T2> constexpr Vec4& operator/= (const Vec3<T2>& v) { x/= v.x; y/= v.y; z/= v.z; return *this; }
    template<typename T2> constexpr Vec4& operator+= (const Vec3<T2>& v) { x+= v.x; y+= v.y; z+= v.z; return *this; }
    template<typename T2> constexpr Vec4& operator-= (const Vec3<T2>& v) { x-= v.x; y-= v.y; z-= v.z; return *this; }
    
    template<typename T2> constexpr auto operator* (const Vec2<T2>& v) const { return Vec4<decltype(x*x)>(x*v.x, y*v.y, z, w); }
    template<typename T2> constexpr auto operator/ (const Vec2<T2>& v) const { return Vec4<decltype(x/x)>(x/v.x, y/v.y, z, w); }
    template<typename T2> constexpr auto operator+ (const Vec2<T2>& v) const { return Vec4<decltype(x+x)>(x+v.x, y+v.y, z, w); }
    template<typename T2> constexpr auto operator- (const Vec2<T2>& v) const { return Vec4<decltype(x-x)>(x-v.x, y-v.y, z, w); }
    
    template<typename T2> constexpr Vec4& operator*= (const Vec2<T2>& v) { x*= v.x; y*= v.y; return *this; }
    template<typename T2> constexpr Vec4& operator/= (const Vec2<T2>& v) { x/= v.x; y/= v.y; return *this; }
    template<typename T2> constexpr Vec4& operator+= (const Vec2<T2>& v) { x+= v.x; y+= v.y; return *this; }
    template<typename T2> constexpr Vec4& operator-= (const Vec2<T2>& v) { x-= v.x; y-= v.y; return *this; }
    
};
