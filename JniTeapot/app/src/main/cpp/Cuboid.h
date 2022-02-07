#pragma once

#include "vec.h"

template<typename T>
struct Cuboid {
    T left, right, 
      bottom, top, 
      near, far;
};