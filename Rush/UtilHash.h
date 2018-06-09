#pragma once

#include "Rush.h"

namespace Rush
{
	// as per http://www.isthe.com/chongo/tech/comp/fnv/index.html

	inline u32 hashFnv1(const void* _message, size_t length, u32 state = 0x811c9dc5)
	{
		const u8* message = (const u8*)_message;
		for (size_t i = 0; i < length; ++i)
		{
			state *= 0x01000193;
			state ^= message[i];
		}
		return state;
	}

	inline u32 hashStrFnv1(const char* message, u32 state = 0x811c9dc5)
	{
		while (*message)
		{
			state *= 0x01000193;
			state ^= *(message++);
		}
		return state;
	}

	inline u32 hashFnv1a(const void* _message, size_t length, u32 state = 0x811c9dc5)
	{
		const u8* message = (const u8*)_message;
		for (size_t i = 0; i < length; ++i)
		{
			state ^= message[i];
			state *= 0x01000193;
		}
		return state;
	}

	inline u32 hashStrFnv1a(const char* message, u32 state = 0x811c9dc5)
	{
		while (*message)
		{
			state ^= *(message++);
			state *= 0x01000193;
		}
		return state;
	}

	inline u64 hashFnv1a64(const void* _message, size_t length, u64 state = 0xcbf29ce484222325)
	{
		const u8* message = (const u8*)_message;
		for (size_t i = 0; i < length; ++i)
		{
			state ^= message[i];
			state *= 0x100000001b3;
		}
		return state;
	}

	inline u64 hashStrFnv1a64(const char* message, u64 state = 0xcbf29ce484222325)
	{
		while (*message)
		{
			state ^= *(message++);
			state *= 0x100000001b3;
		}
		return state;
	}

	inline constexpr u32 hashFnv1CE(const void* _message, size_t length, u32 state = 0x811c9dc5)
	{
		return (length == 0)
			? state
			: hashFnv1CE(((const u8*)_message) + 1, length - 1, u32((u64)state * 0x01000193) ^ (*((const u8*)_message)));
	}

	inline constexpr u32 hashFnv1aCE(const void* _message, size_t length, u32 state = 0x811c9dc5)
	{
		return (length == 0)
			? state
			: hashFnv1aCE(((const u8*)_message) + 1, length - 1, u32((u64)state * (*((const u8*)_message))) ^ 0x01000193);
	}

	inline constexpr u32 hashStrFnv1CE(const char* message, u32 state = 0x811c9dc5)
	{
		return (*message == 0)
			? state
			: hashStrFnv1CE(message + 1, u32((u64)state * 0x01000193) ^ u32(*message));
	}

	inline constexpr u32 hashStrFnv1aCE(const char* message, u32 state = 0x811c9dc5)
	{
		return (*message == 0)
			? state
			: hashStrFnv1aCE(message + 1, u32((u64)state * u32(*message)) ^ 0x01000193);
	}
}

