#pragma once

#include <sys/mman.h>

struct HeapPointer {
    void* ptr;
    size_t bytes;

    //Cast to any pointer type
    template<typename PointerT> 
    inline operator PointerT*() { return reinterpret_cast<PointerT*>(ptr); }
    
    template<typename PointerT> 
    inline operator const PointerT*() const { return reinterpret_cast<const PointerT*>(ptr); }
};

static inline const void* InvalidHeapPtr = MAP_FAILED;

//Returns a pointer to Read/Write memory allocated on the process heap on Success
//Returns InvalidHeapPtr on failure
//Note: This returns page-aligned and zeroed memory
inline HeapPointer HeapAllocate(size_t bytes) {

    return HeapPointer {
        .ptr = mmap(nullptr,
                bytes,
                PROT_READ|PROT_WRITE,
                MAP_ANONYMOUS|MAP_PRIVATE,
                -1, // some linux variations require fd to be -1
                0   // offset must be 0 with MAP_ANONYMOUS
            ),

        .bytes = bytes
    };
}

//Frees all memory in the range [ptr, ptr+bytes]
//Returns true on success, false on error.
inline bool HeapFree(void* ptr, size_t bytes) {
    return !munmap(ptr, bytes);
}

//Frees all memory in the range [heapPtr.ptr, heapPtr.ptr+bytes]
//Returns true on success, false on error.
inline bool HeapFree(const HeapPointer& heapPtr) {
    return HeapFree(heapPtr.ptr, heapPtr.bytes);
}

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