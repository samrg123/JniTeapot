#pragma once

//indirection needed to expand macros
#define _PASTE(x, y) x##y
#define PASTE(x, y) _PASTE(x,y)

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)


#define EVAL(...) __VA_ARGS__
#define CODE(...) EVAL(__VA_ARGS__)

#define _DEFER_EMPTY(...)
#define DEFER(...) __VA_ARGS__ _DEFER_EMPTY()

#ifndef offsetof
#define offsetof(type, member) ((size_t) &(((type*)0)->member))
#endif

#define ASSUME(x) __builtin_assume(x)

//Note: clang uses non-preprocessor string for __FUNCTION__ so we cant concat it with other string literals
#define CALL_SITE_FMT __FILE__ ":" STRINGIFY(__LINE__) " [%s]"

#define COMPILE_COUNTER_VAR(name) kCounter##name##_
#define COMPILE_COUNTER(name) static const auto COMPILE_COUNTER_VAR(name) = __COUNTER__
#define COMPILE_COUNT(counter) (__COUNTER__ - COMPILE_COUNTER_VAR(counter) - 1)