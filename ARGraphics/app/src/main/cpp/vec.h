#pragma once

template<auto kNumDims_>
struct Vec {
    static constexpr auto kNumDims = kNumDims_;
};

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
    static inline const Vec2 zero  = Vec2(1, 1);
    
    constexpr T Area() const { return x*y; }
    constexpr Vec2 Inverse() const { return Vec2(1/x, 1/y); }
    
    template<typename Vt> constexpr auto Dot(const Vt& v) const { return v.x*x + v.y*y; }
    
    constexpr const T* operator& () const { return component; };
    template<typename T2> constexpr operator Vec2<T2>() const { return Vec2<T2>(T2(x), T2(y)); };
    
    template<typename St> constexpr auto operator*  (const St& s) const { return Vec2<decltype(s*x)>(x*s, y*s); }
    template<typename St> constexpr auto operator/  (const St& s) const { return Vec2<decltype(s/x)>(x/s, y/s); }
    template<typename St> constexpr auto operator+  (const St& s) const { return Vec2<decltype(x+s)>(x+s, y+s); }
    template<typename St> constexpr auto operator-  (const St& s) const { return Vec2<decltype(x-s)>(x-s, y-s); }
    
    template<typename St> constexpr Vec2& operator*= (const St& s) { x*= s; y*= s; return *this; }
    template<typename St> constexpr Vec2& operator/= (const St& s) { x/= s; y/= s; return *this; }
    template<typename St> constexpr Vec2& operator+= (const St& s) { x+= s; y+= s; return *this; }
    template<typename St> constexpr Vec2& operator-= (const St& s) { x-= s; y-= s; return *this; }
    
    template<typename T2> constexpr Vec2 operator* (const Vec2<T2>& v) const { return Vec2(x*v.x, y*v.y); }
    template<typename T2> constexpr Vec2 operator/ (const Vec2<T2>& v) const { return Vec2(x/v.x, y/v.y); }
    template<typename T2> constexpr Vec2 operator+ (const Vec2<T2>& v) const { return Vec2(x+v.x, y+v.y); }
    template<typename T2> constexpr Vec2 operator- (const Vec2<T2>& v) const { return Vec2(x-v.x, y-v.y); }
    
    template<typename T2> constexpr Vec2& operator*= (const Vec2<T2>& v) { x*= v.x; y*= v.y; return *this; }
    template<typename T2> constexpr Vec2& operator/= (const Vec2<T2>& v) { x/= v.x; y/= v.y; return *this; }
    template<typename T2> constexpr Vec2& operator+= (const Vec2<T2>& v) { x+= v.x; y+= v.y; return *this; }
    template<typename T2> constexpr Vec2& operator-= (const Vec2<T2>& v) { x-= v.x; y-= v.y; return *this; }
};