#pragma once

#define LOG_ENABLED 1
#if LOG_ENABLED
	
	#include <android/log.h>
	#include <stdio.h>

	#include "jniTeapotUtil.h"
	#include "types.h"
	#include "macros.h"
	
	enum LogLevel:  uint8 { LOG_LEVEL_MSG = 1, LOG_LEVEL_WARN, LOG_LEVEL_ERROR };
	enum LogOption: uint8 { LOG_OPT_FLUSH = 1 };

	constexpr uint32 LogType(LogLevel logLevel, uint8 options = 0, uint16 payload = 0) {
		return (uint32(logLevel)<<24) | (uint32(options)<<16) | payload;
	}

	constexpr android_LogPriority AndroidLogPriority(uint32 logType) {
	    return (logType>>24 == LOG_LEVEL_ERROR) ?  ANDROID_LOG_ERROR :
               (logType>>24 == LOG_LEVEL_WARN)  ?  ANDROID_LOG_WARN :
                                                   ANDROID_LOG_INFO;
	}

	constexpr uint32 kLogTypeEnableMask_ = LogType(LOG_LEVEL_ERROR) | LogType(LOG_LEVEL_WARN) | LogType(LOG_LEVEL_MSG);

	constexpr const char* LogLevelStr(uint32 logType) {
		return 	(logType>>24) == LOG_LEVEL_MSG 	 ? "MSG" :
				(logType>>24) == LOG_LEVEL_WARN  ? "WARN" :
				(logType>>24) == LOG_LEVEL_ERROR ? "ERROR" : "UNKNOWN";
	}


	#define LOG_FMT_(callSiteFmt, fmtStr) callSiteFmt " - %s: { " fmtStr " }\n"
	#define LOG_ARGS_(funcStr, typeStr, ...) funcStr, typeStr, ##__VA_ARGS__

	class Logger;
	extern Logger logger_;

	#define LogEx(type, fmtStr, ...) {											            \
		static constexpr char kFmtStr_[] = LOG_FMT_(CALL_SITE_FMT, fmtStr);		            \
		logger_.PushLogStr<ArrayCount(kFmtStr_), kFmtStr_>(__func__, type, ##__VA_ARGS__);	\
	}

	#define Log(fmt, ...)	LogEx(LogType(LOG_LEVEL_MSG), 	fmt, ##__VA_ARGS__)
	#define Warn(fmt, ...)	LogEx(LogType(LOG_LEVEL_WARN), 	fmt, ##__VA_ARGS__)
	#define Error(fmt, ...) LogEx(LogType(LOG_LEVEL_ERROR), fmt, ##__VA_ARGS__)

	class Logger {
		private:
            static constexpr bool kLogOutputDebug_ 	= true;

			template<typename ...ArgsT>
			void LogStr(uint32 type, const ArgsT&... args) {

				//TODO: add more logging options

				if constexpr(kLogOutputDebug_) {
				    __android_log_print(AndroidLogPriority(type), " => JNI Native Logger", args...);
				}
			}

		public:

			//TODO: introduce Log queue system and run logging on seperate thread - that way we won't introduce as much lag into sound system for example
			template<size_t nFmt, const char (&kLogFmt)[nFmt], typename ...ArgsT>
			void PushLogStr(const char* kFuncStr, uint32 type, const ArgsT&... args) {

				if(type & kLogTypeEnableMask_) {
					// TODO: enable lock when logger runs a seperate thread
					// _logState.mtx.lock();

                    LogStr(type, kLogFmt, LOG_ARGS_(kFuncStr, LogLevelStr(type), args...));

					// _logState.mtx.unlock();
				}
			}

	} logger_;

#else
	#define LogEx(...) 		{}
	#define Log(fmt, ...) 	{}
	#define Warn(fmt, ...) 	{}
	#define Error(fmt, ...) {}
#endif
