#pragma once

#define ASSERT_RUNTIME_ENABLED 1
#define ASSERT_COMPILETIME_ENABLED 1

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
	#define RUNTIME_ASSERT(condition, ...)
#endif
