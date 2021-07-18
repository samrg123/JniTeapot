#pragma once

template<typename T, size_t n> constexpr size_t ArrayCount(const T(&)[n]) { return n; }

template<typename T> constexpr void* ByteOffset(const void* ptr, const T& bytes) { return (char*)ptr + bytes; }
template<typename T> constexpr void* NegByteOffset(const void* ptr, const T& negBytes) { return (char*)ptr - negBytes; }

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

// TODO: see if compiler optimizes this better than memset
template<typename T>
constexpr void ZeroStruct(T* mem) {
    char* memBytes = (char*)mem;
    for(unsigned int i = 0; i < sizeof(T); ++i) memBytes[i] = 0;
}

inline void FillMemory(void* mem, unsigned int value, unsigned int bytes) { __builtin_memset(mem, value, bytes); }

inline void CopyMemory(void *dst, void* src, unsigned int bytes) { __builtin_memcpy(dst, src, bytes); }