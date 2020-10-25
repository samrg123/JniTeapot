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
