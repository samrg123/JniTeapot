#pragma once

#include "Sequence.h"
#include "metaprogrammingUtil.h"

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

        constexpr operator const char* const () const { return str; }
};

template<auto N>
struct ToStringLiteral_ {
    using NType = decltype(N); 

    ToStringLiteral_() = delete;


    static constexpr auto Prefix() {
        if constexpr(N < 0) return StringLiteral("-");
        else return StringLiteral("");
    }

    template<uint64 n>
    static constexpr auto IntegerString() {

        constexpr char c = (n%10) + '0';

        if constexpr(n < 10) return StringLiteral("") + c;
        else return IntegerString<n/10>() + c;
    }

    static constexpr auto ParseInteger() { return Prefix() + IntegerString<Abs(N)>(); }

    static constexpr auto ParseChar() { return StringLiteral("") + N; }

    // TODO: THIS... need to do some floating point divion to compute mantisa and exponent
    static constexpr auto ParseFloat() {
        static_assert(sizeof(NType) == 0, "ToStringLiteral ParseFloat not implemented yet");
        return StringLiteral("float");
    }

    static constexpr auto ParseConstantPointer() { 
        constexpr auto value = N->value;
        using ValueT = decltype(value);

        // Note: Clang doesn't support passing float parameters yet so we handle them here
        if constexpr(IsFloatType<ValueT>()) {

            // TODO: THIS... need to do some foating point divion to compute mantisa and exponent
            return ParseFloat();

        } else if constexpr(IsStringType<ValueT>()) {

            //Parse as string literal    
            return StringLiteral(N->value); 
        
        } else if constexpr(IsNonTypeTemplate<ValueT, StringLiteral>()) {

            //Just return the string literal as is
            return value;

        } else {
         
            //Try to parse as template parameter
            return ToStringLiteral_<value>::Value();
        }
    }

    static constexpr auto ParsePointer() {
        auto value = *N;
        using ValueT = decltype(value);

        //Note: clang doesn't support pasing literal types as template parameters so instead we pass pointers
        //      to 'Constants' which we can further processes as literal type 
        if constexpr(IsTypeTemplate<ValueT, Constant>()) return ParseConstantPointer();
        else {

            // TODO: add support for printing hexidecimal pointers?
            static_assert(sizeof(NType) == 0, "ToStringLiteral pointers not implement yet");
            return StringLiteral("pointer");
        }
    }

    static constexpr auto Value() {

             if constexpr(IsIntegerType<NType>())   return ParseInteger();
        else if constexpr(IsFloatType<NType>())     return ParseFloat();
        else if constexpr(IsPointer<NType>())       return ParsePointer();
        else if constexpr(IsType<NType, char>())    return ParseChar();   
        else                                        return StringLiteral(N);
 
    }
};

template<auto x> 
constexpr auto ToStringLiteral = ToStringLiteral_<x>::Value;

// Note: Clang doesn't support passing float/literal template parameters yet so we use this macro as a fallback to
//       create a unique template instantiation that encodes floatingpoint/literal values
// Note: using static in statement expression doesn't work with gcc instead we can just use 'ToStringLiteral<Contant(x)>()'
#if __INTELLISENSE__
    
    //Note: VsCode Intellisense can't resolve auto template parameters so we throw this hack in to prevent annoying errors
    //TODO: Find out way to get intellisense to deduce template paramters
    #define ToStringLiteral(x) ( static_cast<void>(x), StringLiteral("Intellisense ToStringLiteral"))
#else
    #define ToStringLiteral(x) [](){ return ({ static constexpr auto constant = Constant(x); ToStringLiteral<&constant>(); }); }()
#endif
