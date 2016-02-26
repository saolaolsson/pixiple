#pragma once

#include <array>

#include <d2d1.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#pragma warning(push)
#pragma warning(disable: 4201)

template<std::size_t, typename> class Vector;
using Vector2f = Vector<2, float>;
using Vector2i = Vector<2, int>;
using Vector2u = Vector<2, unsigned>;
using Vector3f = Vector<3, float>;
using Vector3i = Vector<3, int>;
using Vector3u = Vector<3, unsigned>;
using Vector4f = Vector<4, float>;
using Vector4i = Vector<4, int>;
using Vector4u = Vector<4, unsigned>;

template<std::size_t, typename> class Point;
using Point2f = Point<2, float>;
using Point2i = Point<2, int>;
using Point2u = Point<2, unsigned>;
using Point3f = Point<3, float>;
using Point3i = Point<3, int>;
using Point3u = Point<2, unsigned>;

template<std::size_t, typename> class Size;
using Size2f = Size<2, float>;
using Size2i = Size<2, int>;
using Size2u = Size<2, unsigned>;
using Size3f = Size<3, float>;
using Size3i = Size<3, int>;
using Size3u = Size<3, unsigned>;

class Matrix;
class Quaternion;

template <template<std::size_t, typename> typename TO, std::size_t d, typename TI>
std::wostream& operator<<(std::wostream& os, const TO<d, TI>& v) {
	os << L"(";
	for (std::size_t i = 0; i < d-1; i++)
		os << v.e[i] << ", ";
	os << v.e[d-1] << ")";
	return os;
}

// Vector<>

template<std::size_t d, typename T>
class Vector {
public:
	std::array<T, d> e;

	const T& operator[](std::size_t i) const {
		assert(i < d);
		return e[i];
	}

	T& operator[](std::size_t i) {
		assert(i < d);
		return e[i];
	}
};

template<std::size_t d, typename T>
bool operator==(const Vector<d, T>& lhs, const Vector<d, T>& rhs) {
	return std::equal(lhs.e.cbegin(), lhs.e.cend(), rhs.e.cbegin());
}

template<std::size_t d, typename T>
bool operator!=(const Vector<d, T>& lhs, const Vector<d, T>& rhs) {
	return !(lhs == rhs);
}

template<std::size_t d, typename T>
bool operator<(const Vector<d, T>& lhs, const Vector<d, T>& rhs) {
	return std::lexicographical_compare(
		lhs.e.cbegin(), lhs.e.cend(),
		rhs.e.cbegin(), rhs.e.cend());
}

template<std::size_t d, typename T>
Vector<d, T> operator+(const Vector<d, T>& v) {
	return v;
}

template<std::size_t d, typename T>
Vector<d, T> operator-(const Vector<d, T>& v) {
	return static_cast<T>(-1) * v;
}

template<std::size_t d, typename T>
Vector<d, T>& operator+=(Vector<d, T>& lhs, const Vector<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] += rhs.e[i];
	return lhs;
}

template<std::size_t d, typename T>
Vector<d, T>& operator-=(Vector<d, T>& lhs, const Vector<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] -= rhs.e[i];
	return lhs;
}

template<std::size_t d, typename TL, typename TR>
Vector<d, TL>& operator*=(Vector<d, TL>& lhs, const TR rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] *= rhs;
	return lhs;
}

template<std::size_t d, typename TL, typename TR>
Vector<d, TL>& operator/=(Vector<d, TL>& lhs, const TR rhs) {
	assert(rhs != 0);
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] /= rhs;
	return lhs;
}

template<std::size_t d, typename T>
Vector<d, T> operator+(Vector<d, T> lhs, const Vector<d, T>& rhs) {
	return lhs += rhs;
}

template<std::size_t d, typename T>
Vector<d, T> operator-(Vector<d, T> lhs, const Vector<d, T>& rhs) {
	return lhs -= rhs;
}

template<std::size_t d, typename TL, typename TR>
Vector<d, TL> operator*(Vector<d, TL> lhs, const TR rhs) {
	return lhs *= rhs;
}

template<std::size_t d, typename TL, typename TR>
Vector<d, TR> operator*(const TL lhs, Vector<d, TR> rhs) {
	return rhs *= lhs;
}

template<std::size_t d, typename TL, typename TR>
Vector<d, TL> operator/(Vector<d, TL> lhs, const TR rhs) {
	return lhs /= rhs;
}

template<std::size_t d, typename T>
Vector<d, T> operator-(const Point<d, T>& lhs, const Point<d, T>& rhs) {
	Vector<d, T> v;
	for (std::size_t i = 0; i < d; i++)
		v.e[i] = lhs.e[i] - rhs.e[i];
	return v;
}

template<std::size_t d, typename T>
T length(const Vector<d, T>& v) {
	return sqrt(length2(v));
}

template<std::size_t d, typename T>
T length2(const Vector<d, T>& v) {
	T s = 0;
	for (std::size_t i = 0; i < d; i++)
		s += v.e[i] * v.e[i];
	return s;
}

template<std::size_t d, typename T>
Vector<d, T> normalize(const Vector<d, T>& v) {
	return v / length(v);
}

template<std::size_t d, typename T>
bool is_normalized(const Vector<d, T>& v, const T tolerance = static_cast<T>(0)) {
	return abs(length(v) - 1) < tolerance;
}

template<std::size_t d, typename T>
bool equal(const Vector<d, T>& lhs, const Vector<d, T>& rhs, const T tolerance = static_cast<T>(0)) {
	return length(lhs - rhs) < tolerance;
}

template<std::size_t d, typename T>
T dot(const Vector<d, T>& lhs, const Vector<d, T>& rhs) {
	T dp = 0;
	for (std::size_t i = 0; i < d; i++)
		dp += lhs.e[i] * rhs.e[i];
	return dp;
}

template<std::size_t d, typename T>
void swap(Vector<d, T>& lhs, Vector<d, T>& rhs) {
	using namespace std;
	for (std::size_t i = 0; i < d; i++)
		swap(lhs.e[i], rhs.e[i]);
}

template<typename T>
class Vector<2, T> {
public:
	union {
		std::array<T, 2> e;
		struct {
			T x;
			T y;
		};
	};

	Vector() = default;
	Vector(const T x, const T y) : x{x}, y{y} { }
	explicit Vector(DirectX::FXMVECTOR v);

	explicit operator DirectX::XMVECTOR() const;
};

Vector2f operator*(const Vector2f& lhs, const Matrix& rhs);

float length(const Vector2f& v);
Vector2f normalize(const Vector2f& v);
Vector2f orthogonal(const Vector2f& v);

template<typename T>
class Vector<3, T> {
public:
	union {
		std::array<T, 3> e;
		struct {
			T x;
			T y;
			T z;
		};
		Vector<2, T> xy;
	};

	Vector() = default;
	Vector(const T x, const T y, const T z) : x{x}, y{y}, z{z} { }
	Vector(const Vector<2, T>& xy, const T z) : x{xy.x}, y{xy.y}, z{z} { }
	explicit Vector(DirectX::FXMVECTOR v);

	explicit operator DirectX::XMVECTOR() const;
};

Vector3f operator*(const Vector3f& lhs, const Matrix& rhs);
Vector3f operator*(const Vector3f& lhs, const Quaternion& rhs);

float length(const Vector3f& v);
Vector3f normalize(const Vector3f& v);
Vector3f orthogonal(const Vector3f& v);
Vector3f orthogonal(const Vector3f& v1, const Vector3f& v2);
Vector3f vector(const Vector3f& yaw_pitch_roll);
Vector3f vector(const Quaternion& orientation);

template<typename T>
class Vector<4, T> {
public:
	union {
		std::array<T, 4> e;
		struct {
			T x;
			T y;
			T z;
			T w;
		};
		Vector<2, T> xy;
		Vector<3, T> xyz;
	};

	Vector() = default;
	Vector(const T x, const T y, const T z, const T w) : x{x}, y{y}, z{z}, w{w} { }
	Vector(const Vector<3, T>& xyz, const T w) : x{xyz.x}, y{xyz.y}, z{xyz.z}, w{w} { }
	explicit Vector(DirectX::FXMVECTOR v);

	explicit operator DirectX::XMVECTOR() const;
};

Vector4f operator*(const Vector4f& lhs, const Matrix& rhs);
Vector4f operator*(const Vector4f& lhs, const Quaternion& rhs);

float length(const Vector4f& v);
Vector4f normalize(const Vector4f& v);

// Point<>

template<std::size_t d, typename T = float>
class Point {
public:
	std::array<T, d> e;
	
	const T& operator[](std::size_t i) const {
		assert(i < d);
		return e[i];
	}

	T& operator[](std::size_t i) {
		assert(i < d);
		return e[i];
	}
};

template<std::size_t d, typename T>
bool operator==(const Point<d, T>& lhs, const Point<d, T>& rhs) {
	return std::equal(lhs.e.cbegin(), lhs.e.cend(), rhs.e.cbegin());
}

template<std::size_t d, typename T>
bool operator!=(const Point<d, T>& lhs, const Point<d, T>& rhs) {
	return !(lhs == rhs);
}

template<std::size_t d, typename T>
bool operator<(const Point<d, T>& lhs, const Point<d, T>& rhs) {
	return std::lexicographical_compare(
		lhs.e.cbegin(), lhs.e.cend(),
		rhs.e.cbegin(), rhs.e.cend());
}

template<std::size_t d, typename T>
Point<d, T>& operator+=(Point<d, T>& lhs, const Vector<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] += rhs.e[i];
	return lhs;
}

template<std::size_t d, typename T>
Point<d, T>& operator-=(Point<d, T>& lhs, const Vector<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] -= rhs.e[i];
	return lhs;
}

template<std::size_t d, typename T>
Point<d, T>& operator+=(Point<d, T>& lhs, const Size<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] += rhs.e[i];
	return lhs;
}

template<std::size_t d, typename T>
Point<d, T>& operator-=(Point<d, T>& lhs, const Size<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] -= rhs.e[i];
	return lhs;
}

template<std::size_t d, typename T>
Point<d, T> operator+(Point<d, T> lhs, const Vector<d, T>& rhs) {
	return lhs += rhs;
}

template<std::size_t d, typename T>
Point<d, T> operator-(Point<d, T> lhs, const Vector<d, T>& rhs) {
	return lhs -= rhs;
}

template<std::size_t d, typename T>
Point<d, T> operator+(Point<d, T> lhs, const Size<d, T>& rhs) {
	return lhs += rhs;
}

template<std::size_t d, typename T>
Point<d, T> operator-(const Point<d, T> lhs, const Size<d, T>& rhs) {
	return lhs -= rhs;
}

template<std::size_t d, typename T>
T distance2(const Point<d, T> lhs, const Point<d, T>& rhs) {
	T s = 0;
	for (std::size_t i = 0; i < d; i++)
		s += (lhs.e[i] - rhs.e[i]) * (lhs.e[i] - rhs.e[i]);
	return s;
}

template<std::size_t d, typename T>
T distance(const Point<d, T> lhs, const Point<d, T>& rhs) {
	return sqrt(distance2(lhs, rhs));
}

template<std::size_t d, typename T>
bool equal(const Point<d, T>& lhs, const Point<d, T>& rhs, const T tolerance = static_cast<T>(0)) {
	return length(lhs - rhs) < tolerance;
}

template<std::size_t d, typename T>
void swap(Point<d, T>& lhs, Point<d, T>& rhs) {
	using namespace std;
	for (std::size_t i = 0; i < d; i++)
		swap(lhs.e[i], rhs.e[i]);
}

template<typename T>
class Point<2, T> {
public:
	union {
		std::array<T, 2> e;
		struct {
			T x;
			T y;
		};
	};

	Point() = default;
	Point(const T x,  const T y) : x{x}, y{y} { }
	explicit Point(DirectX::FXMVECTOR v);

	operator DirectX::XMVECTOR() const;
};

Point2f operator*(const Point2f& lhs, const Matrix& rhs);

template<typename T>
class Point<3, T> {
public:
	union {
		std::array<T, 3> e;
		struct {
			T x;
			T y;
			T z;
		};
		Point<2, T> xy;
	};

	Point() = default;
	Point(const T x, const T y, const T z) : x{x}, y{y}, z{z} { }
	Point(const Point<2, T> xy, const T z) : x{xy.x}, y{xy.y}, z{z} { }
	explicit Point(DirectX::FXMVECTOR v);

	operator DirectX::XMVECTOR() const;
};

Point3f operator*(const Point3f& lhs, const Matrix& rhs);
Point3f operator*(const Point3f& lhs, const Quaternion& rhs);

// Size<>

template<std::size_t d, typename T = float>
class Size {
public:
	std::array<T, d> e;
	
	const T& operator[](std::size_t i) const {
		assert(i < d);
		return e[i];
	}

	T& operator[](std::size_t i) {
		assert(i < d);
		return e[i];
	}
};

template<std::size_t d, typename T>
bool operator==(const Size<d, T>& lhs, const Size<d, T>& rhs) {
	return std::equal(lhs.e.cbegin(), lhs.e.cend(), rhs.e.cbegin());
}

template<std::size_t d, typename T>
bool operator!=(const Size<d, T>& lhs, const Size<d, T>& rhs) {
	return !(lhs == rhs);
}

template<std::size_t d, typename T>
bool operator<(const Size<d, T>& lhs, const Size<d, T>& rhs) {
	return std::lexicographical_compare(
		lhs.e.cbegin(), lhs.e.cend(),
		rhs.e.cbegin(), rhs.e.cend());
}

template<std::size_t d, typename T>
Size<d, T>& operator+=(Size<d, T>& lhs, const Size<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] += rhs.e[i];
	return lhs;
}

template<std::size_t d, typename T>
Size<d, T>& operator-=(Size<d, T>& lhs, const Size<d, T>& rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] -= rhs.e[i];
	return lhs;
}

template<std::size_t d, typename TL, typename TR>
Size<d, TL>& operator*=(Size<d, TL>& lhs, const TR rhs) {
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] *= rhs;
	return lhs;
}

template<std::size_t d, typename TL, typename TR>
Size<d, TL>& operator/=(Size<d, TL>& lhs, const TR rhs) {
	assert(s != 0);
	for (std::size_t i = 0; i < d; i++)
		lhs.e[i] /= rhs;
	return lhs;
}

template<std::size_t d, typename T>
Size<d, T> operator+(Size<d, T> lhs, const Size<d, T>& rhs) {
	return lhs += rhs;
}

template<std::size_t d, typename T>
Size<d, T> operator-(const Size<d, T> lhs, const Size<d, T>& rhs) {
	return lhs -= rhs;
}

// TODO: missing operators

template<typename T>
class Size<2, T> {
public:
	union {
		std::array<T, 2> e;
		struct {
			T w;
			T h;
		};
		struct {
			T v;
			T h;
		};
	};

	Size() = default;
	Size(const T w, const T h) : w{w}, h{h} { }
	explicit Size(D2D1_SIZE_F s) : w{s.width}, h{s.height} { }
};

template<typename T>
class Size<3, T> {
public:
	union {
		std::array<T, 3> e;
		struct {
			T w;
			T h;
			T d;
		};
		Size<2, T> wh;
	};

	Size() = default;
	Size(const T w, const T h, const T d) : w{w}, h{h}, d{d} { }
	Size(const Size<2, T>& wh, const T d) : w{wh.w}, h{wh.h}, d{d} { }
};

template<typename T>
class Size<4, T> {
public:
	union {
		std::array<T, 4> e;
		struct {
			T t;
			T r;
			T b;
			T l;
		};
	};

	Size() = default;
	Size(const T t, const T l, const T b, const T r) : t{t}, r{r}, b{b}, l{l} { }
};

// Matrix

class Matrix {
public:
	union {
		float e[4][4];
		Vector4f r[4];
	};

	Matrix() = default;
	Matrix(const Vector4f& r0, const Vector4f& r1, const Vector4f& r2, const Vector4f& r3)
		: r{r0, r1, r2, r3} { }
	explicit Matrix(DirectX::CXMMATRIX m);

	explicit operator DirectX::XMMATRIX() const;

	const Vector4f& operator[](std::size_t i) const;
	Vector4f& operator[](std::size_t i);

	static Matrix identity();
	static Matrix translation(const Vector3f& translation);
	static Matrix scale(const Vector3f& scale);
	static Matrix rotation_x(const float angle);
	static Matrix rotation_y(const float angle);
	static Matrix rotation_z(const float angle);
	static Matrix rotation(const Vector3f& axis, const float angle);
	static Matrix projection_transform(const float field_of_view, const float aspect_ratio, const float near_z, const float far_z);
	static Matrix view_transform(const float yaw, const float pitch, const float roll);
};

bool operator==(const Matrix& lhs, const Matrix& rhs);
bool operator!=(const Matrix& lhs, const Matrix& rhs);
bool operator<(const Matrix& lhs, const Matrix& rhs);

Matrix operator+(const Matrix& m);
Matrix operator-(const Matrix& m);

Matrix& operator+=(Matrix& lhs, const Matrix& rhs);
Matrix& operator-=(Matrix& lhs, const Matrix& rhs);
Matrix& operator*=(Matrix& lhs, const Matrix& rhs);
Matrix& operator*=(Matrix& lhs, const float rhs);
Matrix& operator*=(Matrix& lhs, const Quaternion& rhs);
Matrix& operator/=(Matrix& lhs, const float rhs);

Matrix operator+(Matrix lhs, const Matrix& rhs);
Matrix operator-(Matrix lhs, const Matrix& rhs);
Matrix operator*(Matrix lhs, const Matrix& rhs);
Matrix operator*(Matrix lhs, const float rhs);
Matrix operator*(const float lhs, Matrix rhs);
Matrix operator*(Matrix lhs, const Quaternion& rhs);
Matrix operator/(Matrix lhs, const float rhs);

Matrix transpose(const Matrix& m);
Matrix inverse(const Matrix& m);
float determinant(const Matrix& m);

std::wostream& operator<<(std::wostream& os, const Matrix& m);

class Quaternion {
public:
	union {
		std::array<float, 4> e;
		struct {
			float x;
			float y;
			float z;
			float w;
		};
	};

	Quaternion() = default;
	Quaternion(const float x, const float y, float z, float w) : x{x}, y{y}, z{z}, w{w} { }
	explicit Quaternion(DirectX::FXMVECTOR v);
	explicit Quaternion(const Vector3f& axis, const float rotation);
	explicit Quaternion(const float yaw, const float pitch, const float roll);
	explicit Quaternion(const Matrix& m);
	
	explicit operator DirectX::XMVECTOR() const;

	static Quaternion identity();
};

bool operator==(const Quaternion& lhs, const Quaternion& rhs);
bool operator!=(const Quaternion& lhs, const Quaternion& rhs);
bool operator<(const Quaternion& lhs, const Quaternion& rhs);

Quaternion operator+(const Quaternion& q);
Quaternion operator-(const Quaternion& q);

Quaternion& operator*=(Quaternion& lhs, const Quaternion& rhs);
Quaternion& operator*=(Quaternion& lhs, const float rhs);

Quaternion operator*(Quaternion lhs, const Quaternion& rhs);

float length(const Quaternion& q);
float length2(const Quaternion& q);
Quaternion normalize(const Quaternion& q);
Quaternion conjugate(const Quaternion& q);
Quaternion inverse(const Quaternion& q);
Quaternion slerp(const Quaternion& from, const Quaternion& to, const float t);
float dot(const Quaternion& lhs, const Quaternion& rhs);

std::wostream& operator<<(std::wostream& os, const Quaternion& rhs);

class Plane  {
public:
	union {
		std::array<float, 4> e;
		struct {
			float a;
			float b;
			float c;
			float d;
		};
	};

	Plane() = default;
	Plane(const float a, const float b, float c, float d) : a{a}, b{b}, c{c}, d{d} { }
	Plane(const Vector3f& normal, const float d) : Plane{normal.x, normal.y, normal.z, d} {}
	Plane(const Point3f& p1, const Point3f& p2, const Point3f& p3);
	Plane(const Point3f& point, const Vector3f& normal);
	explicit Plane(DirectX::FXMVECTOR v);

	explicit operator DirectX::XMVECTOR() const;

	Vector3f normal() const;
	Point3f point() const;
};

bool operator==(const Plane& lhs, const Plane& rhs);
bool operator!=(const Plane& lhs, const Plane& rhs);
bool operator<(const Plane& lhs, const Plane& rhs);

Plane& operator*=(Plane& lhs, const Matrix& rhs);
Plane& operator*=(Plane& lhs, const Quaternion& rhs);

Plane operator*(Plane lhs, const Matrix& rhs);
Plane operator*(Plane lhs, const Quaternion& rhs);

std::wostream& operator<<(std::wostream& os, const Plane& rhs);

class Colour {
public:
	union {
		std::array<float, 4> e;
		struct {
			float r;
			float g;
			float b;
			float a;
		};
	};

	Colour() = default;
	Colour(const float r, const float g, const float b, float a = 1)
		: r{r}, g{g}, b{b}, a{a} { }
	Colour(const std::uint32_t c)
		:
		a{((c >> 24) & 255) / 255.0f},
		r{((c >> 16) & 255) / 255.0f},
		g{((c >> 8) & 255) / 255.0f},
		b{((c >> 0) & 255) / 255.0f} { }
	explicit Colour(DirectX::FXMVECTOR v);

	explicit operator DirectX::XMVECTOR() const;

	const float* rgba_ptr() const;
	DirectX::PackedVector::XMCOLOR bgra() const;
	DirectX::PackedVector::XMUBYTEN4 rgba() const;
	D2D1_COLOR_F d2d() const;
};

bool operator==(const Colour& lhs, const Colour& rhs);
bool operator!=(const Colour& lhs, const Colour& rhs);
bool operator<(const Colour& lhs, const Colour& rhs);

Colour& operator*=(Colour& lhs, const Colour& rhs);
Colour& operator*=(Colour& lhs, const float rhs);
Colour& operator/=(Colour& lhs, const float rhs);

Colour operator*(Colour lhs, const Colour& rhs);
Colour operator*(Colour lhs, const float rhs);
Colour operator*(const float lhs, Colour rhs);
Colour operator/(Colour lhs, const float rhs);

std::wostream& operator<<(std::wostream& os, const Colour& rhs);

#define RED		Colour(1, 0, 0, 1)
#define GREEN	Colour(0, 1, 0, 1)
#define BLUE	Colour(0, 0, 1, 1)
#define YELLOW	Colour(1, 1, 0, 1)
#define MAGENTA	Colour(1, 0, 1, 1)
#define CYAN	Colour(0, 1, 1, 1)
#define BLACK	Colour(0, 0, 0, 1)
#define GREY	Colour(0.3f, 0.3f, 0.3f, 1)
#define WHITE	Colour(1, 1, 1, 1)

#pragma warning(pop)
