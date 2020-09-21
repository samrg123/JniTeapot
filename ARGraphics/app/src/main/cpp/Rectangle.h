#pragma once

template<typename T>
struct Rectangle {
    T left, top, right, bottom;

    constexpr Rectangle() {};
    constexpr Rectangle(const T& left, const T& top, const T& right, const T& bottom):
                        left(left), top(top), right(right), bottom(bottom) {}
                        
    template<typename T2>
    inline bool Intersects(const Rectangle<T2>& r2) {
        return left <= r2.right  && right  >= r2.left &&
               top  <= r2.bottom && bottom >= r2.top;
    }
    
    template<typename T2>
    inline bool IntersectsArea(const Rectangle<T2>& r2) {
        return left < r2.right  && right  > r2.left &&
               top  < r2.bottom && bottom > r2.top;
    }
    
    inline T Width()  { return right - left; }
    inline T Height() { return bottom - top; }
};