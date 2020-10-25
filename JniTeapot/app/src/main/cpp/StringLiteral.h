#pragma once


#include "Sequence.h"

template<int n>
class StringLiteral {
        template<int n2> friend class StringLiteral;
    
    public:
        const char str[n];
        static constexpr int Length() { return n; }
    
    private:
        
        static inline const auto indexSequence = IncreasingSequence<0, n-2>();
        
        template<int... strIndices>
        constexpr StringLiteral(const char (&str)[n], Sequence<strIndices...>): str{str[strIndices]..., '\0'} {}
        
        template<int n1, int... strIndices>
        constexpr StringLiteral(const char (&str)[n1], Sequence<strIndices...>, char c): str{str[strIndices]..., c, '\0'} {}
        
        template<int n1, int n2, int... str1Indices, int... str2Indices>
        constexpr StringLiteral(const char (&str1)[n1], Sequence<str1Indices...>,
                                const char (&str2)[n2], Sequence<str2Indices...>): str{ str1[str1Indices]..., str2[str2Indices]..., '\0'} {}
        
        constexpr StringLiteral(const char (&str)[1], Sequence<0>): str{'\0'} {}
        constexpr StringLiteral(const char (&str)[1], Sequence<0>, char c): str{c, '\0'} {}
        
        
        constexpr StringLiteral(const char (&str1)[1], Sequence<0>,
                                const char (&str2)[1], Sequence<0>): str{ '\0'} {}
        
        template<int n1, int... str1Indices>
        constexpr StringLiteral(const char (&str1)[n1], Sequence<str1Indices...>,
                                const char (&str2)[1],  Sequence<0>): str{ str1[str1Indices]..., '\0'} {}
        
        template<int n2, int... str2Indices>
        constexpr StringLiteral(const char (&str1)[1],  Sequence<0>,
                                const char (&str2)[n2], Sequence<str2Indices...>): str{ str2[str2Indices]..., '\0'} {}
    
    public:
        
        constexpr StringLiteral(const char (&str)[n]): StringLiteral(str, indexSequence) {}
        
        template<int n2>
        constexpr auto operator+ (const StringLiteral<n2>& s) const { return StringLiteral<n+n2-1>(str, indexSequence, s.str, s.indexSequence); }
        
        template<int n2>
        constexpr auto operator+ (const char (&s)[n2]) const { return StringLiteral<n+n2-1>(str, indexSequence, s, IncreasingSequence<0, n2-2>()); }
        
        constexpr auto operator+ (char c) const { return StringLiteral<n+1>(str, indexSequence, c); }
};

template<typename T, auto n>
struct ToStringLiteral {};

template<unsigned int n>
struct ToStringLiteral<unsigned int, n> {
    static constexpr auto Value() {
        if constexpr(n < 10) return StringLiteral("") + ((n%10)+'0');
        else return ToStringLiteral<unsigned int, n/10>::Value() + ((n%10)+'0');
    }
};

template<>
struct ToStringLiteral<unsigned int, 0u> {
    static constexpr auto Value() { return StringLiteral("0"); }
};

template<int n>
struct ToStringLiteral<int, n> {
    static constexpr auto Value() {
        if constexpr(n < 0) return StringLiteral("-") + ToStringLiteral<unsigned int, (unsigned int)(-n)>::Value();
        else return ToStringLiteral<unsigned int, (unsigned int)n>::Value();
    }
};

#define ToString(x) ToStringLiteral<decltype(x), x>::Value()