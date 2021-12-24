#pragma once

#include "vec.h"

template<typename T>
struct Cuboid {
    T left, right, 
      top, bottom, 
      near, far;
};