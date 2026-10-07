#pragma once

#include "Rush.h"
#include "UtilBuffer.h"
#include "UtilLog.h"
#include <new>
#include <string_view> // declares ranges::enable_borrowed_range/enable_view, lighter than <ranges>
#include <type_traits>
#include <utility>

namespace Rush
{

template <typename T>
class ArrayView
{
public:

	constexpr ArrayView() = default;
	constexpr ArrayView(T* ptr, size_t count)
		: m_data(ptr)
		, m_size(count)
	{
	}
	template <size_t N>
	constexpr ArrayView(T (&arr)[N])
		: m_data(arr)
		, m_size(N)
	{
	}
	// Rvalue ranges only bind to views of const elements, like std::span.
template <typename R>
requires (requires(R& r) { r.data(); r.size(); })
    && (!std::is_same_v<std::remove_cvref_t<R>, ArrayView<T>>)
    && std::is_convertible_v<decltype(std::declval<R&>().data()), T*>
    && (std::is_const_v<T> || std::is_lvalue_reference_v<R>)
	constexpr ArrayView(R&& r)
		: m_data(r.data())
		, m_size(r.size())
	{
	}

	static constexpr ArrayView from(T* ptr, size_t count)
	{
		ArrayView view;
		view.m_data = ptr;
		view.m_size = count;
		return view;
	}

	static ArrayView fromBytes(u8* ptr, size_t count)
	{
		if (!count)
		{
			return {};
		}
		return from(reinterpret_cast<T*>(ptr), count);
	}

template <typename R>
requires (requires(R& r) { r.data(); r.size(); })
    && std::is_convertible_v<decltype(std::declval<R&>().data()), T*>
	static constexpr ArrayView sliceFrom(R& r, size_t start, size_t count)
	{
		const size_t totalSize = size_t(r.size());
		RUSH_ASSERT(start <= totalSize);
		RUSH_ASSERT(start + count <= totalSize);
		return from(r.data() + start, count);
	}

	constexpr ArrayView slice(size_t start, size_t count)
	{
		return sliceFrom(*this, start, count);
	}
	constexpr ArrayView<const T> slice(size_t start, size_t count) const
	{
		return ArrayView<const T>::sliceFrom(*this, start, count);
	}

	constexpr ArrayView subspan(size_t offset) { return slice(offset, m_size - offset); }
	constexpr ArrayView<const T> subspan(size_t offset) const { return slice(offset, m_size - offset); }
	constexpr ArrayView subspan(size_t offset, size_t count) { return slice(offset, count); }
	constexpr ArrayView<const T> subspan(size_t offset, size_t count) const { return slice(offset, count); }
	constexpr ArrayView first(size_t count) { return slice(0, count); }
	constexpr ArrayView<const T> first(size_t count) const { return slice(0, count); }
	constexpr ArrayView last(size_t count) { return slice(m_size - count, count); }
	constexpr ArrayView<const T> last(size_t count) const { return slice(m_size - count, count); }

	constexpr size_t size() const { return m_size; }
	constexpr size_t size_bytes() const { return m_size * sizeof(T); }
	constexpr T* data() { return m_data; }
	constexpr const T* data() const { return m_data; }

	constexpr bool empty() const { return m_size == 0; }
	constexpr T* dataOrNull() { return m_size == 0 ? nullptr : m_data; }
	constexpr const T* dataOrNull() const { return m_size == 0 ? nullptr : m_data; }
	constexpr T& operator[](size_t index)
	{
		RUSH_ASSERT(index < m_size);
		return m_data[index];
	}
	constexpr const T& operator[](size_t index) const
	{
		RUSH_ASSERT(index < m_size);
		return m_data[index];
	}
	constexpr T& front() { return (*this)[0]; }
	constexpr const T& front() const { return (*this)[0]; }
	constexpr T& back() { return (*this)[m_size - 1]; }
	constexpr const T& back() const { return (*this)[m_size - 1]; }
	constexpr T* begin() { return m_data; }
	constexpr T* end() { return m_data + m_size; }
	constexpr const T* begin() const { return m_data; }
	constexpr const T* end() const { return m_data + m_size; }
	constexpr void fill(const T& value)
	{
		for (size_t i = 0; i < m_size; ++i)
		{
			m_data[i] = value;
		}
	}

private:

	T*  m_data = nullptr;
	size_t m_size = 0;
};

template <typename R>
requires requires(R& r) { r.data(); r.size(); }
ArrayView(R&&) -> ArrayView<std::remove_pointer_t<decltype(std::declval<R&>().data())>>;

template <typename T>
class DynamicArray
{
public:

	DynamicArray() = default;

	DynamicArray(size_t size)
		: DynamicArray()
	{
		resize(size);
	}

	DynamicArray(size_t size, const T& defaultValue)
		: DynamicArray()
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		resize(size, defaultValue);
	}

	DynamicArray(const DynamicArray& other)
		: DynamicArray()
	{
		Buffer<T>::reserve(m_buffer, other.m_buffer.m_size);
		Buffer<T>::constructCopyRange(begin(), other.begin(), other.end());
		m_buffer.m_size = other.m_buffer.m_size;
	}

	DynamicArray(DynamicArray&& other) noexcept
		: DynamicArray()
	{
		if (&other != this)
		{
			m_buffer = other.m_buffer;
			other.m_buffer = Buffer<T>();
		}
	}

	DynamicArray& operator = (const DynamicArray& other)
	{
		if (&other != this)
		{
			clear();
			Buffer<T>::reserve(m_buffer, other.m_buffer.m_size);
			Buffer<T>::constructCopyRange(begin(), other.begin(), other.end());
			m_buffer.m_size = other.m_buffer.m_size;
		}
		return *this;
	}

	DynamicArray& operator = (DynamicArray&& other)
	{
		if (&other != this)
		{
			m_buffer = other.m_buffer;
			other.m_buffer = Buffer<T>();
		}
		return *this;
	}


	~DynamicArray()
	{
		Buffer<T>::destroy(m_buffer);
	}

	T&       operator[](size_t i) { return m_buffer.m_data[i]; }
	const T& operator[](size_t i) const { return m_buffer.m_data[i]; }

	T*       data()       { return m_buffer.m_data; }
	const T* data() const { return m_buffer.m_data; }

	T* begin() { return m_buffer.m_data; }
	T* end()   { return m_buffer.m_data + m_buffer.m_size; }

	const T* begin() const { return m_buffer.m_data; }
	const T* end() const { return m_buffer.m_data + m_buffer.m_size; }

	T& front() { return m_buffer.m_data[0]; }
	const T& front() const { return m_buffer.m_data[0]; }

	T& back() { return m_buffer.m_data[m_buffer.m_size-1]; }
	const T& back() const { return m_buffer.m_data[m_buffer.m_size - 1]; }

	size_t size() const { return m_buffer.m_size; }

	ArrayView<T> slice(size_t start, size_t count)
	{
		return ArrayView<T>::sliceFrom(*this, start, count);
	}
	ArrayView<const T> slice(size_t start, size_t count) const
	{
		return ArrayView<const T>::sliceFrom(*this, start, count);
	}

	RUSH_FORCEINLINE void resize(size_t desiredSize)
	{
		Buffer<T>::resize(m_buffer, desiredSize);
	}

	RUSH_FORCEINLINE void resize(size_t desiredSize, const T& defaultValue)
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		Buffer<T>::resize(m_buffer, desiredSize, defaultValue);
	}

	RUSH_FORCEINLINE void reserve(size_t desiredCapacity)
	{
		Buffer<T>::reserve(m_buffer, desiredCapacity);
	}

	RUSH_FORCEINLINE void push(const T& val)
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		Buffer<T>::push(m_buffer, val);
	}

	RUSH_FORCEINLINE void pop()
	{
		Buffer<T>::pop(m_buffer);
	}

	RUSH_FORCEINLINE void clear()
	{
		Buffer<T>::destructRange(begin(), end());
		m_buffer.m_size = 0;
	}

	bool empty() const
	{
		return m_buffer.m_size == 0;
	}

	// Aliases for STL compatibility

	RUSH_FORCEINLINE void push_back(const T& val)
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		Buffer<T>::push(m_buffer, val);
	}

	RUSH_FORCEINLINE void push_back(T&& val)
	{
		Buffer<T>::push(m_buffer, std::move(val));
	}

	RUSH_FORCEINLINE void pop_back()
	{
		Buffer<T>::pop(m_buffer);
	}

private:

	Buffer<T> m_buffer;
};

template <typename T, size_t CAPACITY>
struct StaticArray
{
	StaticArray() : currentSize(0) {}

	T&       operator[](size_t i) { return data[i]; }
	const T& operator[](size_t i) const { return data[i]; }
	const T& back() const { return data[size() - 1]; }
	T&       back() { return data[size() - 1]; }

	bool pushBack(const T& val)
	{
		if (currentSize < CAPACITY)
		{
			data[currentSize] = val;
			++currentSize;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool popBack()
	{
		if (currentSize > 0)
		{
			--currentSize;
			return true;
		}
		else
		{
			return false;
		}
	}

	void pushBackUnsafe(const T& val)
	{
		data[currentSize] = val;
		++currentSize;
	}

	void popBackUnsafe(const T& val) { --currentSize; }

	void clear() { currentSize = 0; }

	size_t        capacity() const { return CAPACITY; }
	size_t        size() const { return currentSize; }
	static size_t elementSize() { return sizeof(T); }

	ArrayView<T> slice(size_t start, size_t count)
	{
		RUSH_ASSERT(start <= currentSize);
		RUSH_ASSERT(start + count <= currentSize);
		return ArrayView<T>::from(data + start, count);
	}
	ArrayView<const T> slice(size_t start, size_t count) const
	{
		RUSH_ASSERT(start <= currentSize);
		RUSH_ASSERT(start + count <= currentSize);
		return ArrayView<const T>::from(data + start, count);
	}

	T*       begin() { return data; }
	T*       end() { return data + currentSize; }
	const T* begin() const { return data; }
	const T* end() const { return data + currentSize; }

	T      data[CAPACITY];
	size_t currentSize;
};

template <typename T, size_t N>
struct alignas(alignof(T)) InlineDynamicArray
{
	RUSH_DISALLOW_COPY_AND_ASSIGN(InlineDynamicArray)

	u8 m_inlineData[sizeof(T) * N];
	T * m_data = nullptr;
	u32 m_count = 0;

	InlineDynamicArray(u32 count)
		: m_data(reinterpret_cast<T*>(m_inlineData))
		, m_count(count)
	{
		if (m_count > N)
		{
			m_data = new T[m_count];
		}
		else
		{
			Buffer<T>::constructRange(m_data, m_data + m_count);
		}
	}

	~InlineDynamicArray()
	{
		if ((void*)m_data == (void*)m_inlineData)
		{
			Buffer<T>::destructRange(m_data, m_data + m_count);
		}
		else
		{
			delete[] m_data;
		}
	}

	T&       operator[](size_t i) { return m_data[i]; }
	const T& operator[](size_t i) const { return m_data[i]; }

	size_t size() const { return m_count; }

	T*       data() { return m_data; }
	const T* data() const { return m_data; }

	ArrayView<T> slice(size_t start, size_t count)
	{
		return ArrayView<T>::sliceFrom(*this, start, count);
	}
	ArrayView<const T> slice(size_t start, size_t count) const
	{
		return ArrayView<const T>::sliceFrom(*this, start, count);
	}
};

}

template <typename T>
constexpr bool std::ranges::enable_borrowed_range<Rush::ArrayView<T>> = true;
template <typename T>
constexpr bool std::ranges::enable_view<Rush::ArrayView<T>> = true;
