#pragma once

#include "mathUtil.h"



#if 0

#include "metaprogrammingUtil.h"

//TODO: TYPEDEF TEMPLATES!!!!!!!
//TODO: Specialize Vector types with neon simd

template<typename T>
struct Vec2Storage {
    union {
        struct { T x, y; };
        T component[2];
    };
    constexpr Vec2Storage(): component{} {}    
};

template<typename T>
struct Vec3Storage {
    union {
        struct { T x, y, z; };
        T component[3];
    };
    constexpr Vec3Storage(): component{} {}    
};

template<typename T>
struct Vec4Storage {
    union {
        struct { T x, y, z, w; };
        T component[4];
    };
    constexpr Vec4Storage(): component{} {}
};

template<typename T_, template<typename> class VecTemplate_, template<typename> class VecStorageTemplate_>
struct VecTemplateParams {
    
    using T = T_;

    template<typename T2> using VecTemplate = VecTemplate_<T2>;
    template<typename T2> using VecStorageTemplate = VecStorageTemplate_<T2>;

    using VecT = VecTemplate<T>;
    using VecStorageT = VecStorageTemplate<T>;
};

template<class VecTemplateParamsT>
struct Vec: VecTemplateParamsT::VecStorageT {
    
    //Flatten template parameter types
    using T           = typename VecTemplateParamsT::T;
    using VecT        = typename VecTemplateParamsT::VecT;
    using VecStorageT = typename VecTemplateParamsT::VecStorageT;

    //Flatten template parameter templates
    template<typename T2> using VecTemplate = typename VecTemplateParamsT::template VecTemplate<T2>;
    
    static inline constexpr auto kNumDims = ArrayCount(VecStorageT{}.component);

    constexpr Vec() {};
    
    //generic construct by array
    template<typename T2, uint8 N>
    constexpr Vec(const T2(&values)[N]) {
    
        //copy over N values
        for(int i = 0; i < Min(kNumDims, N); ++i) {
            this->component[i] = values[i];
        }

        //zero remaining values
        for(int i = N; i < kNumDims; ++i) {
            this->component[i] = 0;
        }
    }

    //generic construct by args
    //TODO: This - make this generic for all vectors 
// private:
//     template<typename T2, int... indicies>
//     constexpr InitVec(Sequece<indicies...>, ) {

//     }

// public:
//     template<typename... T2>
//     constexpr Vec(T2... values) {

//         static_assert(sizeof...(values), "Number of arguments must match number of vector components");

//         constexpr auto sequence = IncreasingSequence<0, kNumDims-1>();
//         this->component[]       
//     }

    //generic cast to VecT
    constexpr operator VecT() const { return VecT(this->component); }

    //generic access operators
    constexpr T& operator[] (uint8 i)       { return this->component[i]; }
    constexpr T  operator[] (uint8 i) const { return this->component[i]; }

    //generic cast to array operators
    using ArrayT = T[kNumDims];
    constexpr operator ArrayT&()             { return this->component; }
    constexpr operator const ArrayT&() const { return this->component; }

    //generic negation operator
    constexpr VecT operator-() const {
        T negComponents[kNumDims];
        for(int i = 0; i < kNumDims; ++i) {
            negComponents[i] = -this->component[i];
        }
        return VecT(negComponents); 
    }

    //generic scaler operators ----------------
    template<typename Ts> constexpr VecT& operator*= (Ts s) { for(T& value : this->component) value*= s; return reinterpret_cast<VecT&>(*this); }
    template<typename Ts> constexpr VecT& operator/= (Ts s) { for(T& value : this->component) value/= s; return reinterpret_cast<VecT&>(*this); }
    template<typename Ts> constexpr VecT& operator+= (Ts s) { for(T& value : this->component) value+= s; return reinterpret_cast<VecT&>(*this); }
    template<typename Ts> constexpr VecT& operator-= (Ts s) { for(T& value : this->component) value-= s; return reinterpret_cast<VecT&>(*this); }

    template<typename Ts> constexpr auto operator*  (Ts s) const { return VecTemplate<decltype(T{}*s)>(*this)*= s; }
    template<typename Ts> constexpr auto operator/  (Ts s) const { return VecTemplate<decltype(T{}*s)>(*this)/= s; }
    template<typename Ts> constexpr auto operator+  (Ts s) const { return VecTemplate<decltype(T{}*s)>(*this)+= s; }
    template<typename Ts> constexpr auto operator-  (Ts s) const { return VecTemplate<decltype(T{}*s)>(*this)-= s; }

    //generic vector operators ----------------
    
    template<auto VectorOp, class VecTemplateParamsT2> 
    constexpr VecT& ApplyVectorOp(const Vec<VecTemplateParamsT2>& v) {
        for(int i = 0; i < Min(kNumDims, v.kNumDims); ++i) {
            VectorOp(this->component[i], v.component[i]);
        }
        return *this;
    }

    template<class ParamT> constexpr VecT& operator*= (const Vec<ParamT>& v) { return ApplyVectorOp<[](T& c1, const typename ParamT::T& c2){ c1*= c2; }>(v); }
    template<class ParamT> constexpr VecT& operator/= (const Vec<ParamT>& v) { return ApplyVectorOp<[](T& c1, const typename ParamT::T& c2){ c1/= c2; }>(v); }
    template<class ParamT> constexpr VecT& operator+= (const Vec<ParamT>& v) { return ApplyVectorOp<[](T& c1, const typename ParamT::T& c2){ c1+= c2; }>(v); }
    template<class ParamT> constexpr VecT& operator-= (const Vec<ParamT>& v) { return ApplyVectorOp<[](T& c1, const typename ParamT::T& c2){ c1-= c2; }>(v); }
    
    // template<typename T2> constexpr auto operator* (const Vec2<T2>& v) const { return Vec2<decltype(x*x)>(x*v.x, y*v.y); }
    // template<typename T2> constexpr auto operator/ (const Vec2<T2>& v) const { return Vec2<decltype(x/x)>(x/v.x, y/v.y); }
    // template<typename T2> constexpr auto operator+ (const Vec2<T2>& v) const { return Vec2<decltype(x+x)>(x+v.x, y+v.y); }
    // template<typename T2> constexpr auto operator- (const Vec2<T2>& v) const { return Vec2<decltype(x-x)>(x-v.x, y-v.y); }
  

    //generic methods -------------

    constexpr T Area() const { 
        T area = 1.f;        
        for(T value : this->component) {
            area*= value;
        }
        return area; 
    }

    template<class ParamT>
    constexpr T Dot(const Vec<ParamT>& v2) const {

        T dot = 0;
        for(int i = 0; i < Min(kNumDims, v2.kNumDims); ++i) {
            dot+= this->component[i] * v2.component[i];
        }
        return dot;
    }

    constexpr T NormSquared() const { Dot(*this); }
    constexpr T Norm()        const { return Sqrt(NormSquared()); };    

    constexpr VecT& Normalize()        { return *this/= Norm(); }
    constexpr VecT  Normalized() const { return *this/  Norm(); }

    constexpr VecT& Invert()        { for(T& value : this->component) value = T(1)/value; }
    constexpr VecT  Inverse() const { return VecT(*this).Invert(); }
};

//Define Vectors ---------------

//Note: clang is really picky about defining a static constexpr member varable of an incomplete type 
//      so we define a VecNBase class which defines VecN functionality and define VecN which defines
//      VecN constants.
//Note: VecNBase uses Vec<T, VecN, VecNStorage> to allow VecN to construct VecN directly  

template<typename T> struct Vec2;

template<typename T>
struct Vec2Base: Vec<VecTemplateParams<T, Vec2, Vec2Storage>> {
    
    using Vec<VecTemplateParams<T, Vec2, Vec2Storage>>::Vec;

    constexpr Vec2Base(T x, T y) {
        this->x = x;
        this->y = y;
    }
};

template<typename T>
struct Vec2: Vec2Base<T> {

    using Vec2Base<T>::Vec2Base;

    static inline constexpr auto up    = Vec2Base<T>( 0,  1);
    static inline constexpr auto down  = Vec2Base<T>( 0, -1);
    static inline constexpr auto left  = Vec2Base<T>(-1,  0);
    static inline constexpr auto right = Vec2Base<T>( 1,  0);
    static inline constexpr auto one   = Vec2Base<T>( 1,  1);
    static inline constexpr auto zero  = Vec2Base<T>( 0,  0);    
};

template<typename T> struct Vec3;

template<typename T>
struct Vec3Base: Vec<VecTemplateParams<T, Vec3, Vec3Storage>> {
    
    using Vec<VecTemplateParams<T, Vec3, Vec3Storage>>::Vec;
    
    constexpr Vec3Base(T x, T y, T z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    template<typename T2> 
    constexpr auto Cross(const Vec3Base<T2>& v) const {

        using ResultT = Vec3Base<decltype(this->x*v.x)>;
        
        return ResultT(this->y*v.z - this->z*v.y, 
                       this->z*v.x - this->x*v.z, 
                       this->x*v.y - this->y*v.x); 
    }    
}; 

template<typename T>
struct Vec3: Vec3Base<T> {
    
    using Vec3Base<T>::Vec3Base;
    
    static inline constexpr auto in    = Vec3Base<T>({ 0,  0,  1});
    static inline constexpr auto out   = Vec3Base<T>({ 0,  0, -1});
    static inline constexpr auto up    = Vec3Base<T>({ 0,  1,  0});
    static inline constexpr auto down  = Vec3Base<T>({ 0, -1,  0});
    static inline constexpr auto left  = Vec3Base<T>({-1,  0,  0});
    static inline constexpr auto right = Vec3Base<T>({ 1,  0,  0});
    static inline constexpr auto one   = Vec3Base<T>({ 1,  1,  1});
    static inline constexpr auto zero  = Vec3Base<T>({ 0,  0,  0});
    

};

template<typename T> struct Vec4;

template<typename T>
struct Vec4Base: Vec<VecTemplateParams<T, Vec4, Vec4Storage>> {

    using Vec<VecTemplateParams<T, Vec4, Vec4Storage>>::Vec; 

    constexpr Vec4Base(T x, T y, T z, T w) {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
    }
};

template<typename T>
struct Vec4: Vec4Base<T> {

    using Vec4Base<T>::Vec4Base;

    static inline constexpr auto in    = Vec4Base<T>({ 0,  0,  1, 0});
    static inline constexpr auto out   = Vec4Base<T>({ 0,  0, -1, 0});
    static inline constexpr auto up    = Vec4Base<T>({ 0,  1,  0, 0});
    static inline constexpr auto down  = Vec4Base<T>({ 0, -1,  0, 0});
    static inline constexpr auto left  = Vec4Base<T>({-1,  0,  0, 0});
    static inline constexpr auto right = Vec4Base<T>({ 1,  0,  0, 0});
    static inline constexpr auto one   = Vec4Base<T>({ 1,  1,  1, 1});
    static inline constexpr auto zero  = Vec4Base<T>({ 0,  0,  0, 0});
};


// constexpr Vec4<float> test1 = Vec4<float>::in;
// constexpr Vec3<float> test2 = Vec3<float>::in;
// constexpr Vec3<float> test3 = Vec3<float>::in.Cross(Vec3<float>::in);


#else

//TODO: TYPEDEF TEMPLATES!!!!!!!

template<auto kNumDims_>
struct Vec {
    static constexpr auto kNumDims = kNumDims_;
};

//Note: not sure if we're going to want to store vectors in a simd/neon registers or something,
//      so I left different dimensions vectors separate

template <typename T, typename Base = Vec<2>>
struct Vec2 : Base {
    
    union {
        struct { T x, y; };
        T component[Base::kNumDims];
    };
    constexpr T& operator[] (uint8 n) { return component[n]; }

    constexpr Vec2() = default;
    constexpr Vec2(const T& x, const T& y): x(x), y(y) {}
 
    static inline const Vec2 up    = Vec2(0, 1);
    static inline const Vec2 down  = Vec2(0, -1);
    static inline const Vec2 left  = Vec2(-1, 0);
    static inline const Vec2 right = Vec2(1, 0);
    static inline const Vec2 one   = Vec2(1, 1);
    static inline const Vec2 zero  = Vec2(0, 0);
    
    constexpr T Area() const { return x*y; }
    constexpr Vec2 Inverse() const { return Vec2(1/x, 1/y); }
    
    constexpr T     NormSquared() const { return x*x + y*y; }
    constexpr T     Norm()        const { return Sqrt(NormSquared()); };

    constexpr Vec2 operator-() const { return Vec3(-x, -y); }
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
    
    constexpr Vec3() = default;
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
    
    constexpr T     Area()        const { return x*y*z; }
    constexpr Vec3  Inverse()     const { return Vec3(1/x, 1/y, 1/z); }
    constexpr T     NormSquared() const { return x*x + y*y + z*z; }
    constexpr T     Norm()        const { return Sqrt(NormSquared()); };
    constexpr Vec3  Abs()         const { return Vec3(FastAbs(x), FastAbs(y), FastAbs(z)); };
    
    constexpr Vec3& Normalize() { return *this/= Norm(); }
    
    constexpr Vec3 operator-() const { return Vec3(-x, -y, -z); }
    template<typename T2> constexpr auto Dot(const Vec2<T2>& v)   const { return v.x*x + v.y*y; }
    template<typename T2> constexpr auto Dot  (const Vec3<T2>& v) const { return v.x*x + v.y*y + v.z*z; }
    template<typename T2> constexpr auto Cross(const Vec3<T2>& v) const { return ::Vec3(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x); }
    
    constexpr const T* operator& () const { return component; };
    constexpr operator Vec2<T>()    const { return *(Vec2<T>*)this; };
    constexpr operator Vec2<T>&()         { return *(Vec2<T>*)this; };
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
    
    constexpr Vec4() = default;
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
    
    constexpr T     Area()        const { return x*y*z*w; }
    constexpr Vec4  Inverse()     const { return Vec4(1/x, 1/y, 1/z, 1/w); }
    constexpr T     NormSquared() const { return x*x + y*y + z*z + w*w; }
    constexpr T     Norm()        const { return Sqrt(NormSquared()); };
    
    constexpr Vec4 operator-() const { return Vec4(-x, -y, -z, -w); }
    template<typename T2> constexpr auto Dot(const Vec2<T2>& v) const { return v.x*x + v.y*y; }
    template<typename T2> constexpr auto Dot(const Vec3<T2>& v) const { return v.x*x + v.y*y + v.z*z; }
    template<typename T2> constexpr auto Dot(const Vec4<T2>& v) const { return v.x*x + v.y*y + v.z*z + v.w*w; }
    
    constexpr Vec4& Normalize() { return *this/= Norm(); }
    
    constexpr const T* operator& () const { return component; };
    constexpr operator Vec3<T>() const    { return *(Vec3<T>*)this; };
    constexpr operator Vec2<T>() const    { return *(Vec2<T>*)this; };
    constexpr operator Vec3<T>&()         { return *(Vec3<T>*)this; };
    constexpr operator Vec2<T>&()         { return *(Vec2<T>*)this; };
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

#endif