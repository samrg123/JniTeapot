#pragma once

template<typename T> constexpr bool LargerThan8Bit(const T& n)  { return n & (T)(~0xFF); }
template<typename T> constexpr bool LargerThan16Bit(const T& n) { return n & (T)(~0xFFFF); }
template<typename T> constexpr bool LargerThan32Bit(const T& n) { return n & (T)(~0xFFFFFFFF); }


//TODO: implement non-compare min/max
template<typename T>
constexpr auto Min(const T& n) { return n; }

template<typename T1, typename T2, typename... TOpt>
constexpr auto Min(const T1& n1, const T2& n2, const TOpt&... nOpt) { return n1 < n2 ? Min(n1, nOpt...) : Min(n2, nOpt...); }

template<typename T>
constexpr auto Max(const T& n) { return n; }

template<typename T1, typename T2, typename... TOpt>
constexpr auto Max(const T1& n1, const T2& n2, const TOpt&... nOpt) { return n1 > n2 ? Max(n1, nOpt...) : Max(n2, nOpt...); }

template<typename T>
constexpr auto Abs(const T& n) { return n >= 0 ? n : -n; }

template<typename T> constexpr bool IsPow2(T n)     { return (n&(n-1)) == 0; }
template<typename T> constexpr bool IsPow2Safe(T n) { return n && IsPow2(n); }

template<>
inline bool IsPow2<float>(float n) { return (unsigned int&)n & (0x1FF<<23); }

template<> //TODO: add Nan and inf detection
inline bool IsPow2Safe<float>(float n) { return IsPow2(n); }

auto constexpr Pi() { return 3.141592653589793238462643383279;  }
template <typename T> constexpr auto ToDegrees(const T& v) { return v * (180.f/Pi()); }
template <typename T> constexpr auto ToRadians(const T& v) { return v * (Pi()/180.f); }

inline float FastCos(float r) { return __builtin_cosf(r); }
inline float FastSin(float r) { return __builtin_sinf(r); }
inline float FastTan(float r) { return __builtin_tanf(r); }

constexpr float Infinity() { return __builtin_inff(); }
inline bool IsInfinity(float n) { return __builtin_isinf(n); }

//Note: These assume ArmV7 or newer with native floating point
inline short    ILog2(float n) { return ((int&)n >> 23) - 127; }
constexpr short ILog2Safe(float n) { return n ? ILog2(n) : 0; }

inline unsigned int ISqrtPow2(unsigned int n) { return 1 << (ILog2(n)>>1); }
constexpr unsigned int ISqrtPow2Safe(unsigned int n) { return n > 0 ? ISqrtPow2(n) : 0; }

// Note: arm doesn't have any built in sqrt
// TODO: benchmark Sqrt against FastSqrt
inline float    Sqrt(float n)          { return __builtin_sqrtf(n); }
inline float    FastSqrt(float n)      { return (float&)( ((unsigned int&)n+= (127<<23))>>= 1 ); }
constexpr float FastSqrtSafe(float n)  { return n ? FastSqrt(n) : 0; }

inline float FastAbs(float n) { return (float&)( (int&)n&= (~(1<<31)) ); }
inline int   FastAbs(int n)   { return __builtin_abs(n); }
template<typename T> inline bool Approx(T a, T b, T epsilon = T(.00001f))  { return FastAbs(a-b) <= epsilon; }

inline float FastPow10(float n) {
    // Note this is: 10^n = 2^(log_2(10)*n)
    // 60801 is fine tunning parameter for lowest rounding error taken from "A Fast, Compact Approximation of the Exponential Function" (scaled by 8 for double->float conversion)
    // Note exponent bit shifting and offset is done in floating point to avoid zeroing lower 23 bits in int
    int tmp = n*3.32192809489f*(1<<23) + ((127<<23) - 8*60801);
    return (float&)tmp;
}

inline float FastPow10(int n) {
    
    //Note: float exponent is in range [-126, 127] so floating point is in range [2*2^-126, 2*2^127] = [2.35e-38, 3.4e38]
    //      instead of doing any math we just lookup the answer in a 79 entry table
    static const float pow10Table[] = {
        0.f,
        1e-38f, 1e-37f, 1e-36f, 1e-35f, 1e-34f, 1e-33f, 1e-32f, 1e-31f, 1e-30f, 1e-29f,
        1e-28f, 1e-27f, 1e-26f, 1e-25f, 1e-24f, 1e-23f, 1e-22f, 1e-21f, 1e-20f, 1e-19f,
        1e-18f, 1e-17f, 1e-16f, 1e-15f, 1e-14f, 1e-13f, 1e-12f, 1e-11f, 1e-10f, 1e-09f,
        1e-08f, 1e-07f, 1e-06f, 1e-05f, 1e-04f, 1e-03f, 1e-02f, 1e-01f, 1e+00f, 1e+01f,
        1e+02f, 1e+03f, 1e+04f, 1e+05f, 1e+06f, 1e+07f, 1e+08f, 1e+09f, 1e+10f, 1e+11f,
        1e+12f, 1e+13f, 1e+14f, 1e+15f, 1e+16f, 1e+17f, 1e+18f, 1e+19f, 1e+20f, 1e+21f,
        1e+22f, 1e+23f, 1e+24f, 1e+25f, 1e+26f, 1e+27f, 1e+28f, 1e+29f, 1e+30f, 1e+31f,
        1e+32f, 1e+33f, 1e+34f, 1e+35f, 1e+36f, 1e+37f, 1e+38f,
        Infinity()
    };
    
    //convert n to clamped index in pow10Table
    //Note: if(Abs(n) <= 38) n+=39; else { if(n < 0) n = 0; else n = 78; }
    
    //Warn: if right shift is not arithmetic and instead fills upper bits with 0, replace s with ((n>>31)^1)*78
    int s = (n>>31)&78; // 78 if n is negative, 0 otherwise
    n+= 39;
    n-= (((38+39) - FastAbs(n))>>31)*(n - s);
    
    return pow10Table[n];
}

constexpr int Round(float n) { return int(n + .5f); }
constexpr int IPart(float n) { return int(n);}
constexpr float FPart(float n) { return n - IPart(n);}


template<typename T1, typename T2>
constexpr auto RoundFraction(const T1& numerator, const T2& denominator) { return (numerator + (denominator>>1))/denominator; }

template<typename T1, typename T2>
constexpr auto CeilFraction(const T1& numerator, const T2& denominator) { return (numerator + denominator-1)/denominator; }

template<typename T1, typename T2, typename T3>
constexpr bool InRange(const T1& n, const T2& min, const T3& max) { return n >= min && n <= max; }

template<typename T>
constexpr T Pow2RoundUp(T n) {
    --n;

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wshift-count-overflow"
    switch(sizeof(T)) {
        case 8: n|= n>>32;
        case 4: n|= n>>16;
        case 2: n|= n>>8;
        default: {
            n|= n>>4;
            n|= n>>2;
            n|= n>>1;
        }
    }
    #pragma clang diagnostic pop
    
    ++n;
    return n;
}

template<typename T>
constexpr T Pow2RoundDown(T n) { return IsPow2(n) ? n : Pow2RoundUp(n)>>1; }

template<>
inline float Pow2RoundDown<float>(float n) { return (float&)(((unsigned int&)n&= (0x1FF<<23))); }

template<>
inline float Pow2RoundUp<float>(float n) { return IsPow2(n) ? n : (float&)(((unsigned int&)n&= (0x1FF<<23))+= (1<<23) ); }


//template <typename T>
//inline unsigned char IthBitPosition(const T& n, unsigned char i) {
//
//    //NOTE: adopted from: https://graphics.stanford.edu/~seander/bithacks.html#SelectPosFromMSBRank
//
//    // sum groups of 2 bits
//    // s1 = (n & b0101...) + ((n >> 1) & b0101...);
//    // Note: subtraction is optimization [works for all bit pairs: {00 01 10 11}]
//    constexpr auto kSum1Mask = ~T(0)/3U;
//    auto s1 =  n - ((n >> 1) & kSum1Mask);
//
//    // sum groups of 4 bits
//    // s2 = (s1 & b0011...) + ((s1 >> 2) & b0011...);
//    constexpr auto kSum2Mask = ~T(0)/5U;
//    auto s2 = (s1 & kSum2Mask) + ((s1 >> 2) & kSum2Mask);
//
//    // sum groups of 8 bits
//    // s3 = (s2 & 0x0f0f...) + ((s2 >> 4) & 0x0f0f...);
//    // Note: we can skip (s2 & 0x0f0f...) because max s2 group sum is 4 ( {10}+{10} = {0100} )
//    //       we still need to mask the result so unused groups don't overflow
//    //       Ex. prevent xx10 from overflow: '{00}{00}{10}{10}' -> '{0000}xx10{0100}'
//    constexpr auto kSum3Mask = ~T(0)/0x11U;
//    auto s3 = (s2 + (s2 >> 4)) & kSum3Mask;
//
//    // sum groups of 16 bits
//    // s4 = (s3 & 0x00ff...) + ((s3 >> 8) & 0x00ff...);
//    constexpr auto kSum4Mask = ~T(0)/0x101U;
//    auto s4 = (s3 + (s3 >> 8)) & kSum4Mask;
//
//    // sum groups of 32 bits
//    auto bitSum = (s4 >> 32) + (s4 >> 48);
//
//    // Now do branchless select!
//    constexpr unsigned char kNumBits = sizeof(T)*8;
//    unsigned char bitPos = kNumBits;
//
//    // TODO: Figure out this black magic below and then genralize with template for different sizes
//
//    // if (i > bitSum) {bitPos-= 32; i-= bitSum;}
//    bitPos-= ((bitSum - i) & 256) >> 3; i-= bitSum & ((bitSum - i) >> 8);
//
//    // if(s4 == 19) bitSum = 4; else if(s4 == 20) bitSum = 1; else bitSum = 0;
//    bitSum = (s4 >> (s4 - 16)) & 0xff;
//
//    // if (r > t) {s -= 16; r -= t;}
//    bitPos-= ((bitSum - i) & 256) >> 4; i-= (bitSum & ((bitSum - i) >> 8));
//    bitSum = (s3 >> (bitPos - 8)) & 0xf;
//
//    // if (r > t) {s -= 8; r -= t;}
//    bitPos-= ((bitSum - i) & 256) >> 5; i-= (bitSum & ((bitSum - i) >> 8));
//    bitSum = (s2 >> (bitPos - 4)) & 0x7;
//
//    // if (r > t) {s -= 4; r -= t;}
//    bitPos-= ((bitSum - i) & 256) >> 6; i-= (bitSum & ((bitSum - i) >> 8));
//    bitSum = (s1 >> (bitPos - 2)) & 0x3;
//
//    // if (r > t) {s -= 2; r -= t;}
//    bitPos-= ((bitSum - i) & 256) >> 7; i-= (bitSum & ((bitSum - i) >> 8));
//    bitSum = (n >> (bitPos - 1)) & 0x1;
//
//    // if (r > t) s--;
//    bitPos-= ((bitSum - i) & 256) >> 8;
//    bitPos = kNumBits - bitPos;
//
//    return bitPos;
//}