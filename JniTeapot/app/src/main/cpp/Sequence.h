#pragma once

//Note: build times are too slow using Sequence below so we use stdlib O(n) implementation for now
#include <utility>
#include "customAssert.h"

template<int... n>
using Sequence = std::integer_sequence<int, n...>;

template<int start, int end>
constexpr auto IncreasingSequence() {
    COMPILE_ASSERT(start == 0, "Start is non zero!");
    return std::make_integer_sequence<int, end+1>();
}

#if 0

//TODO: make these O(n) by halving each time
template<int... N> struct Sequence {
    
    template<int... N2>
    static constexpr auto Concat(Sequence<N2...>) { return Sequence<N..., N2...>(); }
};

template<int start, int end>
constexpr auto DecreasingSequence() {
    
    //Note: 'if constexpr' is required - prevents both sides of statement being evaluated at compiletime
    if constexpr(start > end) return Sequence<start>::Concat(DecreasingSequence<start-1, end>());
    else return Sequence<start>();
}

template<int start, int end>
constexpr auto IncreasingSequence() {
    
    //Note: 'if constexpr' is required - prevents both sides of statement being evaluated at compiletime
    if constexpr(start < end) return Sequence<start>::Concat(IncreasingSequence<start+1, end>());
    else return Sequence<start>();
}

#endif