#pragma once

#include "types.h"
#include "mathUtil.h"

template<auto n> constexpr auto StrCount(const char(&)[n]) 		{ return n-1; }
template<auto n> constexpr auto StrCount(const wchar_t(&)[n]) 	{ return n-1; }

constexpr char LowerCase(char c) { return c | (1<<5);}
constexpr char UpperCase(char c) { return c & (~(1<<5));}

template<typename T>
inline T* SkipLine(T* str) {
    while(*str && *str++ != '\n');
    return str;
}

constexpr bool IsWhiteSpace(char c) {
    switch(c) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\f': return true;
    }
    return false;
}

template<typename T>
inline T* SkipWhiteSpace(T* str) {
    for(char c; (c = *str) && IsWhiteSpace(c); ++str);
    return str;
} 

inline auto StrSign(char* str, char** strEnd = nullptr) {
    
    switch(*str) {
        case '-': {
            if(strEnd) *strEnd = str+1;
            return -1;
        }
        
        case '+': {
            if(strEnd) *strEnd = str+1;
            return 1;
        }
            
        //if there's no sign number is positive
        default: {
            if(strEnd) *strEnd = str;
            return 1;
        }
    }
}

template<typename T>
inline T StrDigits(char* str, T maxValue, char** strEnd = nullptr) {
    
    //skip over leading 0's
    while(*str == '0') ++str;
    
    //grab digits
    T digits = 0;
    for(char c; InRange((c = *str), '0', '9'); ++str) {
        
        //Note: we check in separate variable to prevent overflow
        T newDigits = 10*digits + (c - '0');
        if(newDigits > maxValue) break;
        
        digits = newDigits;
    }
    
    if(strEnd) *strEnd = str;
    return digits;
}

inline int32 StrToInt(char* str, char** strEnd = nullptr) {
    str = SkipWhiteSpace(str);
    int sign = StrSign(str, &str);
    return sign*StrDigits(str, MaxInt32(), strEnd);
}

inline float StrToFloat(char* str, char** strEnd = nullptr) {
    
    str = SkipWhiteSpace(str);
    int sign = StrSign(str, &str),
        digits = StrDigits(str, 0xFFFFFF, &str); //get leading digits
    
    //get number of digits to decimal point
    char* tmpStr = str;
    while(InRange(*str, '0', '9')) ++str;
    int power = str - tmpStr;
    
    //check for decimal point
    if(*str == '.') {
        ++str;
        
        //add fractional sigfigs
        tmpStr = str;
        for(char c; InRange((c = *str), '0', '9') && digits < 0xFFFFFF; ++str) {
            digits = 10*digits + (c - '0');
        }
        power-= str - tmpStr;
        
        //ignore remaining digits
        while(InRange(*str, '0', '9')) ++str;
    }
    
    //check for E
    if(LowerCase(*str) == 'e') power+= StrToInt(++str, &str);
    
    if(strEnd) *strEnd = str;
    return (sign*digits)*FastPow10(power);
}