#pragma once

#include <cstdlib>
#include "macros.h"

// Note: log.h may use Panic so we prototype it before we define it
template<size_t n, const char(&kPanicFmt)[n], typename ...ArgsT>
[[noreturn]] void Panic_(const ArgsT&... args);

// Note: LOG_FMT_ expansion is defined and defined in log.h
#define PANIC_FMT_(errorCallSiteFmt, errorMsgFmt) LOG_FMT_(errorCallSiteFmt, "\n\tPANIC CALLED! {\n\t\tMSG: " errorMsgFmt "\n\t}\n")

#define Panic(errorMsgFmt, ...) { 												\
	static constexpr char kPanicFmt[] = PANIC_FMT_(CALL_SITE_FMT, errorMsgFmt); \
	Panic_<ArrayCount(kPanicFmt), kPanicFmt>(__func__, ##__VA_ARGS__); 			\
}

#include "log.h"

template<size_t n, const char(&kPanicFmt)[n], typename ...ArgsT>
[[noreturn]] void Panic_(const char* const kErrorFuncStr, const ArgsT&... args) {

	#if LOG_ENABLED
		logger_.PushLogStr<n, kPanicFmt>(kErrorFuncStr, LogType(LOG_LEVEL_ERROR), args...);
	#endif

	exit(1);
}
