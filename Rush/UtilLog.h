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

#ifdef __GNUC__
#define RUSH_LOG(text, ...)         {Rush::Log::message(text, ##__VA_ARGS__);}
#define RUSH_LOG_WARNING(text, ...) {Rush::Log::warning(text, ##__VA_ARGS__); if(Rush::Log::breakOnWarning) RUSH_BREAK;}
#define RUSH_LOG_ERROR(text, ...)   {Rush::Log::error(text, ##__VA_ARGS__); if(Rush::Log::breakOnError) RUSH_BREAK;}
#define RUSH_LOG_FATAL(text, ...)   {Rush::Log::fatal(text, ##__VA_ARGS__); RUSH_BREAK;}
#else
#define RUSH_LOG(text, ...)         {Rush::Log::message(text, __VA_ARGS__);}
#define RUSH_LOG_WARNING(text, ...) {Rush::Log::warning(text, __VA_ARGS__); if(Rush::Log::breakOnWarning) RUSH_BREAK;}
#define RUSH_LOG_ERROR(text, ...)   {Rush::Log::error(text, __VA_ARGS__); if(Rush::Log::breakOnError) RUSH_BREAK;}
#define RUSH_LOG_FATAL(text, ...)   {Rush::Log::fatal(text, __VA_ARGS__); RUSH_BREAK;}
#endif

#if (defined(RUSH_DEBUG) || defined(FORCE_ASSERTS) || defined(RUSH_FORCE_ASSERTS))
#define RUSH_ASSERT(v)               { if (!(v)){RUSH_LOG_FATAL("Assert '" #v "' failed in '%s'.", RUSH_FUNCTION);} }
#ifdef __GNUC__
#define RUSH_ASSERT_MSG(v, msg, ...) { if (!(v)){RUSH_LOG_FATAL("Assert '" #v "' failed in '%s'. " ## msg, RUSH_FUNCTION, ##__VA_ARGS__);} }
#else
#define RUSH_ASSERT_MSG(v, msg, ...) { if (!(v)){RUSH_LOG_FATAL("Assert '" #v "' failed in '%s'. " ## msg, RUSH_FUNCTION, __VA_ARGS__);} }
#endif
#else
#define RUSH_ASSERT(v) RUSH_UNUSED(v)
#define RUSH_ASSERT_MSG(v, msg, ...) RUSH_UNUSED(v)
#endif

}
