#pragma once

#include "mathUtil.h"
#include "memUtil.h"
#include "stringUtil.h"

constexpr unsigned int RGBA(uint r, uint g, uint b, uint a = 255) { return (r << 24) | (g << 16) | (b << 8) | a; }
constexpr unsigned int RGBA(float r, float g, float b, float a = 1.f) { return (Round(r*255.f) << 24) | (Round(g*255.f) << 16) | (Round(b*255.f) << 8) | Round(a*255.f); }

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


template<typename T> void Swap(T& x, T& y) {
	T tmp = (T&&)x;
	x = (T&&)y;
	y = (T&&)tmp;
}

template<typename T>
constexpr auto InvokedResult(const T& func) { return func(); }

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
