#pragma once

#include "Rush.h"

#include "Platform.h"

namespace Rush
{
struct Log
{
	typedef void (*LogMessageCallback)(const char* msg);

	static void debug(const char* msg, ...);
	static void message(const char* msg, ...);
	static void warning(const char* msg, ...);
	static void error(const char* msg, ...);
	static void fatal(const char* msg, ...);

	static bool breakOnWarning;
	static bool breakOnError;

	static LogMessageCallback callbackDebug;
	static LogMessageCallback callbackMessage;
	static LogMessageCallback callbackWarning;
	static LogMessageCallback callbackError;
	static LogMessageCallback callbackFatal;

	static const char* prefixMessage;
	static const char* prefixWarning;
	static const char* prefixError;
	static const char* prefixFatal;
};

#if (defined(RUSH_DEBUG) || defined(FORCE_ASSERTS) || defined(RUSH_FORCE_ASSERTS))
#define RUSH_STRINGIFY_FOR_ASSERTS(x) #x
#define RUSH_TOSTRING_FOR_ASSERTS(x) RUSH_STRINGIFY_FOR_ASSERTS(x)
#define RUSH_ASSERT(v)                                                                                                 \
	if (!(v))                                                                                                          \
		switch (Platform_MessageBox(__FILE__ "(" RUSH_TOSTRING_FOR_ASSERTS(__LINE__) ")\nFunction: " RUSH_FUNCTION     \
		                                                                             "\nExpression: " #v,              \
		    "Assert"))                                                                                                 \
		{                                                                                                              \
		case MessageBoxResult_Retry: RUSH_BREAK; break;                                                                \
		case MessageBoxResult_Abort: Platform_TerminateProcess(1);                                                     \
		default: break;                                                                                                \
		}
#define RUSH_ASSERT_MSG(v, msg)                                                                                        \
	if (!(v))                                                                                                          \
		switch (Platform_MessageBox(__FILE__ "(" RUSH_TOSTRING_FOR_ASSERTS(                                            \
		                                __LINE__) ")\nFunction: " RUSH_FUNCTION "\nExpression: " #v "\nMessage: " msg, \
		    "Assert"))                                                                                                 \
		{                                                                                                              \
		case MessageBoxResult_Retry: RUSH_BREAK; break;                                                                \
		case MessageBoxResult_Abort: Platform_TerminateProcess(1);                                                     \
		default: break;                                                                                                \
		}
#define RUSH_ERROR RUSH_BREAK
#else
#define RUSH_ASSERT(v) RUSH_UNUSED((v))
#define RUSH_ASSERT_MSG(v, msg)
#define RUSH_ERROR
#endif

#ifdef __GNUC__
#define RUSH_LOG(text, ...)         {Rush::Log::message(text, ##__VA_ARGS__);}
#define RUSH_LOG_WARNING(text, ...) {Rush::Log::warning(text, ##__VA_ARGS__); if(Rush::Log::breakOnWarning) RUSH_BREAK;}
#define RUSH_LOG_ERROR(text, ...)   {Rush::Log::error(text, ##__VA_ARGS__); if(Rush::Log::breakOnError) RUSH_BREAK;}
#define RUSH_LOG_FATAL(text, ...)   {Rush::Log::fatal(text, ##__VA_ARGS__); if(Rush::Log::breakOnError) RUSH_BREAK;}
#else
#define RUSH_LOG(text, ...)         {Rush::Log::message(text, __VA_ARGS__);}
#define RUSH_LOG_WARNING(text, ...) {Rush::Log::warning(text, __VA_ARGS__); if(Rush::Log::breakOnWarning) RUSH_BREAK;}
#define RUSH_LOG_ERROR(text, ...)   {Rush::Log::error(text, __VA_ARGS__); if(Rush::Log::breakOnError) RUSH_BREAK;}
#define RUSH_LOG_FATAL(text, ...)   {Rush::Log::fatal(text, __VA_ARGS__); if(Rush::Log::breakOnError) RUSH_BREAK;}
#endif

}
