#pragma once

#include "types.h"

struct NoCopyClass {
	NoCopyClass() = default;
	NoCopyClass(const NoCopyClass&) = delete;
	NoCopyClass(const NoCopyClass&&) = delete;
	NoCopyClass& operator= (const NoCopyClass&) = delete;
	NoCopyClass& operator= (const NoCopyClass&&) = delete;
};

template<typename T>
struct Constant {
    const T value;
    constexpr Constant(const T& value): value(value) {}

    constexpr operator const T&() { return value; }
};

// Specialization for Arrays
template<typename T, size_t n>
struct Constant<T[n]> {
    T value[n];
    
    constexpr Constant(const T (&value_)[n]) {
        for(size_t i = 0; i < n; ++i) value[i] = value_[i];
    }

    constexpr operator const T&() { return value; }
};


struct TrueType  { constexpr operator bool() { return true; } };
struct FalseType { constexpr operator bool() { return false; } };

template<typename Type, template<typename...> class Template>
struct IsTypeTemplate: FalseType {};

template<typename Type, template<typename...> class Template>
struct IsTypeTemplate<const Type, Template>: IsTypeTemplate<Type, Template> {};

template<typename... ParamT, template<typename...> class Template>
struct IsTypeTemplate<Template<ParamT...>, Template>: TrueType {};

template<typename Type, template<auto...> class Template>
struct IsNonTypeTemplate: FalseType {};

template<typename Type, template<auto...> class Template>
struct IsNonTypeTemplate<const Type, Template>: IsNonTypeTemplate<Type, Template> {};

template<auto... ParamT, template<auto...> class Template>
struct IsNonTypeTemplate<Template<ParamT...>, Template>: TrueType {};

template<typename T1, typename T2> struct IsType: FalseType {};
template<typename T> struct IsType<T, T>: TrueType {};

template<typename T> struct IsPointer: FalseType {};
template<typename T> struct IsPointer<T*>: TrueType {};

template<typename T> struct IsIntegerType: FalseType {};
template<typename T> struct IsIntegerType<const T>: IsIntegerType<T> {}; 

template<> struct IsIntegerType<int8> : TrueType {};
template<> struct IsIntegerType<int16>: TrueType {};
template<> struct IsIntegerType<int32>: TrueType {};
template<> struct IsIntegerType<int64>: TrueType {};

template<> struct IsIntegerType<uint8> : TrueType {};
template<> struct IsIntegerType<uint16>: TrueType {};
template<> struct IsIntegerType<uint32>: TrueType {};
template<> struct IsIntegerType<uint64>: TrueType {};

template<typename T> struct IsFloatType: FalseType{};
template<typename T> struct IsFloatType<const T>: IsFloatType<T> {};

template<> struct IsFloatType<float> : TrueType {};
template<> struct IsFloatType<double>: TrueType {};

template<typename T> struct IsStringType: FalseType {};

template<> struct IsStringType<char*>			 : TrueType {};
template<> struct IsStringType<const char*>		 : TrueType {};
template<> struct IsStringType<char* const>		 : TrueType {};
template<> struct IsStringType<const char* const>: TrueType {};