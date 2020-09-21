#pragma once

template<typename T>
constexpr auto Min(const T& n) { return n; }

template<typename T1, typename T2, typename... TOpt>
constexpr auto Min(const T1& n1, const T2& n2, const TOpt&... nOpt) { return n1 < n2 ? Min(n1, nOpt...) : Min(n2, nOpt...); }

template<typename T>
constexpr auto Max(const T& n) { return n; }

template<typename T1, typename T2, typename... TOpt>
constexpr auto Max(const T1& n1, const T2& n2, const TOpt&... nOpt) { return n1 > n2 ? Max(n1, nOpt...) : Max(n2, nOpt...); }

template<typename T>
constexpr bool IsPow2(T n) { return (n&(n-1)) == 0; }

template<typename T>
constexpr bool IsPow2Safe(T n) { return n && IsPow2(n); }

template<>
inline bool IsPow2<float>(float n) { return (unsigned int&)n & (0x1FF<<23); }

template<> //TODO: add Nan and inf detection
inline bool IsPow2Safe<float>(float n) { return IsPow2(n); }

template<typename T>
constexpr void* ByteOffset(const void* ptr, const T& bytes) { return (char*)ptr + bytes; }
constexpr auto ByteDistance(const void* ptr1, const void* ptr2) { return (char*)ptr1 - (char*)ptr2; }

template <typename T>
constexpr bool IsAlignedPow2(const T* ptr, size_t n) { return !((size_t)ptr & (n-1)); }

template <typename T>
constexpr T* AlignedDownPow2(const T* ptr, size_t n) {
    //Note: ptr & ~(n-1)
    return (T*)((size_t)ptr & -n);
}

template<typename T>
constexpr size_t AlignUpOffsetPow2(const T* ptr, size_t n) {
	// Note: size_t remainder = (size_t&)ptr & (n-1); return remainder ? (n - remainder) : 0;
	size_t nMinusOne = n-1;
	return (n - ((size_t&)ptr & nMinusOne)) & nMinusOne;
}

template<typename T, auto n> constexpr auto ArrayCount(const T(&)[n]) { return n; }
template<auto n> constexpr auto StrCount(const char(&)[n]) 		{ return n-1; }
template<auto n> constexpr auto StrCount(const wchar_t(&)[n]) 	{ return n-1; }

struct NoCopyClass {
	NoCopyClass() = default;
	NoCopyClass(const NoCopyClass&) = delete;
	NoCopyClass(const NoCopyClass&&) = delete;
	NoCopyClass& operator= (const NoCopyClass&) = delete;
	NoCopyClass& operator= (const NoCopyClass&&) = delete;
};

template<typename T> void Swap(T& x, T& y) {
	T tmp = (T&&)x;
	x = (T&&)y;
	y = (T&&)tmp;
}

// TODO: see if compiler optimizes this
template<typename T>
constexpr void ZeroStruct(T* mem) {
	char* memBytes = (char*)mem;
	for(unsigned int i = 0; i < sizeof(T); ++i) memBytes[i] = 0;
}

//Note: These assume ArmV7 or newer with native floating point
inline short ILog2(float n) { return ((int&)n >> 23) - 127; }
inline short ILog2Safe(float n) { return n ? ILog2(n) : 0; }

inline unsigned int ISqrtPow2(unsigned int n) { return 1 << (ILog2(n)>>1); }
inline unsigned int ISqrtPow2Safe(unsigned int n) { return n > 0 ? ISqrtPow2(n) : 0; }

// Note: arm doesn't have any built in sqrt
inline float FastSqrt(float n) { return (float&)( ((unsigned int&)n+= (127<<23))>>= 1 ); }
inline float FastSqrtSafe(float n) { return n ? FastSqrt(n) : 0; }

inline float FastAbs(float n) { return (float&)( (int&)n&= (~(1<<31)) ); }

constexpr int Round(float n) { return int(n + .5f); }
constexpr int IPart(float n) { return int(n);}
constexpr float FPart(float n) { return n - IPart(n);}


template<typename T1, typename T2>
constexpr auto RoundFraction(const T1& numerator, const T2& denominator) { return (numerator + (denominator>>1))/denominator; }

template<typename T1, typename T2>
constexpr auto CeilFraction(const T1& numerator, const T2& denominator) { return (numerator + denominator-1)/denominator; }

template<typename T1, typename T2, typename T3>
inline bool InRange(const T1& n, const T2& min, const T3& max) { return n >= min && n <= max; }

template<typename T>
inline T Pow2RoundUp(T n) {
	--n;

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
	
	++n;
	return n;
}

template<typename T>
inline T Pow2RoundDown(T n) { return IsPow2(n) ? n : Pow2RoundUp(n)>>1; }

template<>
inline float Pow2RoundDown<float>(float n) { return (float&)(((unsigned int&)n&= (0x1FF<<23))); }

template<>
inline float Pow2RoundUp<float>(float n) { return IsPow2(n) ? n : (float&)(((unsigned int&)n&= (0x1FF<<23))+= (1<<23) ); }


//Note: Returns smallest index that is not less than element in data
template<typename E_t, typename D_t, typename N_t, typename L_t>
inline N_t BinarySearchLower(const E_t& element, const D_t* data, N_t size,
							 const L_t& LessThan = [](auto n1, auto n2) { return n1 < n2; }) {
	
	N_t index = 0;
	while(size > 0) {
		
		//Note: this searches even indices more than once, but saves a conditional branch
		//EX: {1, 2, [3], 4} will split to {1, 2} {3, 4} and may recheck 3
		N_t halfSize = size>>1;
		if(LessThan(data[index + halfSize], element)) index+= size - halfSize;
		size = halfSize;
	}
	
	return index;
}

//Note: Returns smallest index that is greater than element
template<typename E_t, typename D_t, typename N_t, typename L_t>
inline N_t BinarySearchUpper(const E_t& element, const D_t* data, N_t size,
							 const L_t& LessThan = [](auto n1, auto n2) { return n1 < n2; }) {
	
	return BinarySearchLower(element, data, size, [&](auto n1, auto n2){ return !LessThan(n2, n1); });
}

constexpr unsigned int RGBA(uint r, uint g, uint b, uint a = 255) { return (r << 24) | (g << 16) | (b << 8) | a; }
constexpr unsigned int RGBA(float r, float g, float b, float a = 1.f) { return (Round(r*255.f) << 24) | (Round(g*255.f) << 16) | (Round(b*255.f) << 8) | Round(a*255.f); }

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
