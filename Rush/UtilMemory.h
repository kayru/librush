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

template <typename T> class UniquePtr
{
public:
	UniquePtr() = default;
	explicit UniquePtr(T* in_p) : p(in_p) {}
	UniquePtr(nullptr_t*) : p(nullptr) {}
	UniquePtr(const UniquePtr&)            = delete;
	UniquePtr(UniquePtr&& other) : p(other.p)
	{
		other.p = nullptr;
	}
	UniquePtr& operator=(const UniquePtr&) = delete;
	UniquePtr& operator=(UniquePtr&& other)
	{
		if (p != other.p)
		{
			delete p;
			p = other.p;
			other.p = nullptr;
		}
		return *this;
	}
	~UniquePtr() { delete p; }

	T*       operator->() { return p; }
	const T* operator->() const { return p; }
	T*       get() { return p; }
	const T* get() const { return p; }

private:
	T* p = nullptr;
};

}
