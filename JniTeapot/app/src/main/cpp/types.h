#pragma once

#include <stdint.h>

using int64 = int64_t;
using int32 = int32_t;
using int16 = int16_t;
using int8  = int8_t;
using uint64 = uint64_t;
using uint32 = uint32_t;
using uint16 = uint16_t;
using uint8  = uint8_t;

using uint 	 = unsigned int;
using uchar  = unsigned char;
using ushort  = unsigned short;
using ulong  = unsigned long;
using ulonglong  = unsigned long long;

constexpr uint64 MaxUint64() { return uint64(-1LL); }
constexpr uint32 MaxUint32() { return uint32(-1LL); }
constexpr uint16 MaxUint16() { return uint16(-1LL); }
constexpr uint8  MaxUint8()  { return uint8(-1LL); }

constexpr int64 MaxInt64() { return MaxUint64()>>1; }
constexpr int32 MaxInt32() { return MaxUint32()>>1; }
constexpr int16 MaxInt16() { return MaxUint16()>>1; }
constexpr int8  MaxInt8()  { return MaxUint8()>>1; }

constexpr size_t 	MaxSizeT() 	   { return size_t(-1LL); }
constexpr uchar 	MaxUChar() 	   { return uchar(-1LL); }
constexpr ulong 	MaxULong() 	   { return ulong(-1LL); }
constexpr ulonglong MaxULongLong() { return ulonglong(-1LL); }

template<typename T> constexpr auto KB(T x) { return x*1024; }
template<typename T> constexpr auto MB(T x) { return KB(x)*1024; }
template<typename T> constexpr auto GB(T x) { return MB(x)*1024; }
template<typename T> constexpr auto TB(T x) { return GB(x)*1024; }

template<class rType, class ... paramTypes>
using FuncPtr = rType (*)(paramTypes...);

#define CrtGlobalInitFunc 	__attribute((constructor(102))) void
#define CrtGlobalPreInitFunc __attribute__((constructor(101))) void