#pragma once

#define ASSERT_COMPILETIME_ENABLED 1
#if OPTIMIZED_BUILD
	#define ASSERT_RUNTIME_ENABLED 0
#else
	#define ASSERT_RUNTIME_ENABLED 1
#endif

#if ASSERT_COMPILETIME_ENABLED
	#define COMPILE_ASSERT(condition, ...) static_assert(condition, ##__VA_ARGS__)
#else
	#define COMPILE_ASSERT(condition, ...)
#endif

// Note included last so log.h has access to all other macros
#if ASSERT_RUNTIME_ENABLED

	#define RUNTIME_ASSERT(condition, msg, ...) {                                  										            \
		const bool conditionVal = condition;                                              									        \
		if(!conditionVal) {                                                         									            \
			Error("\n\tRuntime assertion Failed {\n\t\tMSG: [" msg "]\n\t\tcondition: [" #condition "]\n\t}\n", ##__VA_ARGS__); 	\
			__builtin_trap(); /*send SIGTRAP to debugger*/																            \
		}                                                                           									            \
	}

	// Note: included after definition so log can still use assertions
	#include "log.h"
#else
    //Note: __builtin_assume doesn't evaluate condition so we do it beforehand;
	#define RUNTIME_ASSERT(condition, ...) { bool condition_ = (condition);  __builtin_assume(condition_); }
#endif
