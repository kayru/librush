#pragma once

namespace Rush
{
template <typename T> struct Tuple2
{
	T x, y;

	T*            begin() { return &x; }
	T*            end() { return &y + 1; }
	const T*      begin() const { return &x; }
	const T*      end() const { return &y + 1; }
	static size_t size() { return 2; }

	bool operator==(const Tuple2<T>& rhs) const { return x == rhs.x && y == rhs.y; }
	bool operator!=(const Tuple2<T>& rhs) const { return x != rhs.x || y != rhs.y; }
};

template <typename T> struct Tuple3
{
	T x, y, z;

	T*            begin() { return &x; }
	T*            end() { return &z + 1; }
	const T*      begin() const { return &x; }
	const T*      end() const { return &z + 1; }
	static size_t size() { return 3; }

	bool operator==(const Tuple3<T>& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
	bool operator!=(const Tuple3<T>& rhs) const { return x != rhs.x || y != rhs.y || z != rhs.z; }
};

template <typename T> struct Tuple4
{
	T x, y, z, w;

	T*            begin() { return &x; }
	T*            end() { return &w + 1; }
	const T*      begin() const { return &x; }
	const T*      end() const { return &w + 1; }
	static size_t size() { return 4; }

	bool operator==(const Tuple4<T>& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }
	bool operator!=(const Tuple4<T>& rhs) const { return x != rhs.x || y != rhs.y || z != rhs.z || w != rhs.w; }
};

typedef Tuple2<float> Tuple2f;
typedef Tuple3<float> Tuple3f;
typedef Tuple4<float> Tuple4f;

typedef Tuple2<int> Tuple2i;
typedef Tuple3<int> Tuple3i;
typedef Tuple4<int> Tuple4i;

typedef Tuple2<unsigned> Tuple2u;
typedef Tuple3<unsigned> Tuple3u;
typedef Tuple4<unsigned> Tuple4u;

typedef Tuple2<bool> Tuple2b;
typedef Tuple3<bool> Tuple3b;
typedef Tuple4<bool> Tuple4b;
}
