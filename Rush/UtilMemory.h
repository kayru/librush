#pragma once

namespace Rush
{

inline void* allocateBytes(size_t sizeBytes)
{
	return new char[sizeBytes];
}

inline void deallocateBytes(void* ptr)
{
	delete [] (char*)ptr;
}

}
