#pragma once

#define LOG_ENABLED 1
#if LOG_ENABLED
	
	#include "macros.h"
	#include "util.h"
	#include "types.h"
	#include "memUtil.h"

	#include <android/log.h>
	#include <stdio.h>
	
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

			//Note: LOGGER_ENTRY_MAX_PAYLOAD is defined in liblog/include/log/log.h to be 4068
			//		this limits the maximum number of bytes __android_log_print can log and will
			//		truncate messages longer than it. Because liblog is built with the kernel we have
			//		no way of telling what the actual maximum is? We spilt android log calls into 
			//		multiples of kLogcatMaxPayloadBytes to prevent logs from being truncated.
			//Note: We use kLogcatMaxPayloadBytes=4000 instead of 4068 to allow future revisions of android 
			//		to add metadata to the log which shrinks the maximum payload bytes.
			//TODO: Find out if there is actually a way to query what LOGGER_ENTRY_MAX_PAYLOAD is!	
			//TODO: Make sure that LOGGER_ENTRY_MAX_PAYLOAD doesn't also include sizeof(kLogTag)-1
			//TODO: Why doesn't logcat spit out 4k chars... only spits out 1024!?! 
			static constexpr uint32 kLogcatMaxPayloadBytes = 1024;
			static constexpr const char* kLogcatTag = " => JNI Native Logger";

			template<typename ...ArgsT>
			void LogStr(uint32 type, const char* fmt, const ArgsT&... args) {

				//TODO: add more logging options
				//TODO: look into __android_log_set_logger API 30

				if constexpr(kLogOutputDebug_) {

					auto logPriority = AndroidLogPriority(type);
					uint32 logBytes = snprintf(nullptr, 0, fmt, args...)+1;

					if(logBytes <= kLogcatMaxPayloadBytes) __android_log_print(logPriority, kLogcatTag, fmt, args...);
					else {

						//attempt to allocate buffer
						char* logBuffer = HeapAllocate(logBytes);

						//Just truncate the log for now
						//TODO: find way to print this using a fixed buffer
						if(logBuffer == InvalidHeapPtr) {
							__android_log_print(logPriority, kLogcatTag, fmt, args...);
							return;
						}

						//print log string
						snprintf(logBuffer, logBytes, fmt, args...);

						//Split the log string into logcat chunks
						char* logBufferEnd = logBuffer + logBytes;   
						for(char* logBufferPtr = logBuffer;;) {
							
							uint32 logBufferPtrAdvance;
							uint32 logBufferPtrBreakpoint;

							//search for whitespace to split the log
							uint32 whitespaceBreakpoint = [&]() {
								
								for(uint32 i = kLogcatMaxPayloadBytes; i; --i) {	
									if(IsWhiteSpace(logBufferPtr[i])) return i;
								}
	
								return 0u;
							}();

							if(whitespaceBreakpoint) {
								
								//Jump over whitespace in next loop
								logBufferPtrAdvance = whitespaceBreakpoint+1;
								logBufferPtrBreakpoint = whitespaceBreakpoint;

							} else {

								//search for nice charter to break on
								logBufferPtrBreakpoint = [&]() {

									for(uint32 i = kLogcatMaxPayloadBytes; i; --i) {	
										switch(logBuffer[i]) {
											case '.':
											case '-': return i;
										}
									}

									//Didn't find a nice character just split on max size
									return kLogcatMaxPayloadBytes; 
								}();
								logBufferPtrAdvance = logBufferPtrBreakpoint;
							}

							//replace breakpoint with null
							char logBufferPtrBreakpointChar = logBufferPtr[logBufferPtrBreakpoint];
							logBufferPtr[logBufferPtrBreakpoint] = '\0';
							
							//write to logcat
							__android_log_write(logPriority, kLogcatTag, logBufferPtr);
					
							//restore breakpoint
							logBufferPtr[logBufferPtrBreakpoint] = logBufferPtrBreakpointChar;
						
							//advance logBufferPtr
							logBufferPtr+= logBufferPtrAdvance;

							//check to see if we're done
							uint32 remainingLogBytes = ByteDistance(logBufferEnd, logBufferPtr); 
							if(remainingLogBytes <= kLogcatMaxPayloadBytes) {
								
								//do the last copy
								__android_log_write(logPriority, kLogcatTag, logBufferPtr);
								break;
							}
						}

						HeapFree(logBuffer, logBytes);
					}


				}
			}

		public:

			//TODO: introduce Log queue system and run logging on separate thread - that way we won't introduce as much lag into sound system for example
			template<size_t nFmt, const char (&kLogFmt)[nFmt], typename ...ArgsT>
			void PushLogStr(const char* kFuncStr, uint32 type, const ArgsT&... args) {

				if(type & kLogTypeEnableMask_) {
					// TODO: enable lock when logger runs a separate thread
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


template<typename T>
inline void LogType() { Log("\n\t-- TYPE: %s --\n", __PRETTY_FUNCTION__ ); }