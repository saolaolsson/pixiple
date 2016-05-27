#include "vector.h"

using namespace DirectX;

// Vector2f

Vector2f::Vector(FXMVECTOR v) {
	XMStoreFloat2(reinterpret_cast<XMFLOAT2*>(this), v);
}

Vector2f::operator XMVECTOR() const {
	XMFLOAT2 v{x, y};
	return XMLoadFloat2(&v);
}

Vector2f operator*(const Vector2f& lhs, const Matrix& rhs) {
	return static_cast<Vector2f>(XMVector2TransformNormal(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMMATRIX>(rhs)));
}

float length(const Vector2f& v) {
	return XMVectorGetX(XMVector2Length(
		static_cast<XMVECTOR>(v)));
}

Vector2f normalize(const Vector2f& v) {
	return static_cast<Vector2f>(XMVector2Normalize(
		static_cast<XMVECTOR>(v)));
}

Vector2f orthogonal(const Vector2f& v) {
	return {-v.y, v.x};
}

// Vector3f

Vector3f::Vector(FXMVECTOR v) {
	XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(this), v);
}

Vector3f::operator XMVECTOR() const {
	XMFLOAT3 v{x, y, z};
	return XMLoadFloat3(&v);
}

Vector3f operator*(const Vector3f& lhs, const Matrix& rhs) {
	return static_cast<Vector3f>(XMVector3TransformNormal(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMMATRIX>(rhs)));
}

Vector3f operator*(const Vector3f& lhs, const Quaternion& rhs) {
	return static_cast<Vector3f>(XMVector3Rotate(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMVECTOR>(rhs)));
}

float length(const Vector3f& v) {
	return XMVectorGetX(XMVector3Length(
		static_cast<XMVECTOR>(v)));
}

Vector3f normalize(const Vector3f& v) {
	return static_cast<Vector3f>(XMVector3Normalize(
		static_cast<XMVECTOR>(v)));
}

Vector3f orthogonal(const Vector3f& v) {
	if (abs(v.x) > abs(v.y))
		return {-v.z, 0, v.x}; // v cross {0, 1, 0}
	else
		return {0, v.z, -v.y}; // v cross {1, 0, 0}
}

Vector3f orthogonal(const Vector3f& lhs, const Vector3f& rhs) {
	return {
		lhs.y*rhs.z - lhs.z*rhs.y,
		lhs.z*rhs.x - lhs.x*rhs.z,
		lhs.x*rhs.y - lhs.y*rhs.x};
}

Vector3f vector(const Vector3f& yaw_pitch_roll) {
	return vector(Quaternion{yaw_pitch_roll.y, yaw_pitch_roll.x, yaw_pitch_roll.z});
}

Vector3f vector(const Quaternion& orientation) {
	return Vector3f{0, 0, 1} * orientation;
}

// Vector4f

Vector4f::Vector(FXMVECTOR v) {
	XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(this), v);
}

Vector4f::operator XMVECTOR() const {
	XMFLOAT4 v{x, y, z, w};
	return XMLoadFloat4(&v);
}

float length(const Vector4f& v) {
	return XMVectorGetX(XMVector4Length(
		static_cast<XMVECTOR>(v)));
}

Vector4f normalize(const Vector4f& v) {
	return static_cast<Vector4f>(XMVector4Normalize(
		static_cast<XMVECTOR>(v)));
}

Vector4f operator*(const Vector4f& lhs, const Matrix& rhs) {
	return static_cast<Vector4f>(XMVector4Transform(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMMATRIX>(rhs)));
}

Vector4f operator*(const Vector4f& lhs, const Quaternion& rhs) {
	return static_cast<Vector4f>(XMVectorSelect(
		g_XMIdentityR3,
		XMVector3Rotate(static_cast<XMVECTOR>(lhs), static_cast<XMVECTOR>(rhs)),
		g_XMSelect1110));
}

// Point2f

Point2f::Point(FXMVECTOR v) {
	XMStoreFloat2(reinterpret_cast<XMFLOAT2*>(this), v);
}

Point2f::operator XMVECTOR() const {
	XMFLOAT2 v{x, y};
	return XMLoadFloat2(&v);
}

Point2f operator*(const Point2f& lhs, const Matrix& rhs) {
	return static_cast<Point2f>(XMVector2TransformCoord(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMMATRIX>(rhs)));
}

// Point3f

Point3f::Point(FXMVECTOR v) {
	XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(this), v);
}

Point3f::operator XMVECTOR() const {
	XMFLOAT3 v{x, y, z};
	return XMLoadFloat3(&v);
}

Point3f operator*(const Point3f& lhs, const Matrix& rhs) {
	return static_cast<Point3f>(XMVector3TransformCoord(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMMATRIX>(rhs)));
}

Point3f operator*(const Point3f& lhs, const Quaternion& rhs) {
	return static_cast<Point3f>(XMVector3Rotate(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMVECTOR>(rhs)));
}

// Matrix

Matrix::Matrix(CXMMATRIX m) {
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(this), m);
}

Matrix::operator XMMATRIX() const {
	return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(this));
}

const Vector4f& Matrix::operator[](size_t i) const {
	return r[i];
}

Vector4f& Matrix::operator[](size_t i) {
	return r[i];
}

Matrix Matrix::identity() {
	return {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1}};
}

Matrix Matrix::translation(const Vector3f& translation) {
	return static_cast<Matrix>(XMMatrixTranslation(translation.x, translation.y, translation.z));
}

Matrix Matrix::scale(const Vector3f& scale) {
	return static_cast<Matrix>(XMMatrixScaling(scale.x, scale.y, scale.z));
}

Matrix Matrix::rotation_x(const float angle) {
	return static_cast<Matrix>(XMMatrixRotationX(angle));
}

Matrix Matrix::rotation_y(const float angle) {
	return static_cast<Matrix>(XMMatrixRotationY(angle));
}

Matrix Matrix::rotation_z(const float angle) {
	return static_cast<Matrix>(XMMatrixRotationZ(angle));
}

Matrix Matrix::rotation(const Vector3f& axis, const float angle) {
	return static_cast<Matrix>(XMMatrixRotationAxis(
		static_cast<XMVECTOR>(axis),
		angle));
}

Matrix Matrix::projection_transform(const float field_of_view, const float aspect_ratio, const float near_z, const float far_z) {
	return static_cast<Matrix>(XMMatrixPerspectiveFovLH(
		field_of_view, aspect_ratio, near_z, far_z));
}

Matrix Matrix::view_transform(const float yaw, const float pitch, const float roll) {
	return static_cast<Matrix>(XMMatrixRotationRollPitchYaw(pitch, yaw, roll));
}

bool operator==(const Matrix& lhs, const Matrix& rhs) {
	if (lhs.r[0] != rhs.r[0])
		return false;
	else if (lhs.r[1] != rhs.r[1])
		return false;
	else if (lhs.r[2] != rhs.r[2])
		return false;
	else if (lhs.r[3] != rhs.r[3])
		return false;
	else
		return true;
}

bool operator!=(const Matrix& lhs, const Matrix& rhs) {
	return !(lhs == rhs);
}

bool operator<(const Matrix& lhs, const Matrix& rhs) {
	for (int i = 0; i < 4; i++) {
		if (lhs[i] < rhs[i])
			return true;
		else if (!(lhs[i] == rhs[i]))
			return false;
	}
	return false;
}

Matrix operator+(const Matrix& m) {
	return m;
}

Matrix operator-(const Matrix& m) {
	return {-m[0], -m[1], -m[2], -m[3]};
}

Matrix& operator+=(Matrix& lhs, const Matrix& rhs) {
	return lhs = {
		lhs[0] + rhs[0],
		lhs[1] + rhs[1],
		lhs[2] + rhs[2],
		lhs[3] + rhs[3]};
}

Matrix& operator-=(Matrix& lhs, const Matrix& rhs) {
	return lhs = {
		lhs[0] - rhs[0],
		lhs[1] - rhs[1],
		lhs[2] - rhs[2],
		lhs[3] - rhs[3]};
}

Matrix& operator*=(Matrix& lhs, const Matrix& rhs) {
	return lhs = static_cast<Matrix>(XMMatrixMultiply(
		static_cast<XMMATRIX>(lhs),
		static_cast<XMMATRIX>(rhs)));
}

Matrix& operator*=(Matrix& lhs, const Quaternion& rhs) {
	return lhs = static_cast<Matrix>(XMMatrixMultiply(
		static_cast<XMMATRIX>(lhs),
		XMMatrixRotationQuaternion(static_cast<XMVECTOR>(rhs))));
}

Matrix& operator*=(Matrix& lhs, const float rhs) {
	return lhs = {lhs[0] * rhs, lhs[1] * rhs, lhs[2] * rhs, lhs[3] * rhs};
}

Matrix& operator/=(Matrix& lhs, const float rhs) {
	assert(rhs != 0);
	return lhs *= 1/rhs;
}

Matrix operator+(Matrix lhs, const Matrix& rhs) {
	return lhs += rhs;
}

Matrix operator-(Matrix lhs, const Matrix& rhs) {
	return lhs -= rhs;
}

Matrix operator*(Matrix lhs, const Matrix& rhs) {
	return lhs *= rhs;
}

Matrix operator*(Matrix lhs, const Quaternion& rhs) {
	return lhs *= rhs;
}

Matrix operator*(Matrix lhs, const float rhs) {
	return lhs *= rhs;
}

Matrix operator*(const float lhs, Matrix rhs) {
	return rhs *= lhs;
}

Matrix operator/(Matrix lhs, const float rhs) {
	return lhs *= 1/rhs;
}

Matrix transpose(const Matrix& m) {
	return static_cast<Matrix>(XMMatrixTranspose(
		static_cast<XMMATRIX>(m)));
}

Matrix inverse(const Matrix& m) {
	return static_cast<Matrix>(XMMatrixInverse(
		nullptr,
		static_cast<XMMATRIX>(m)));
}

float determinant(const Matrix& m) {
	return XMVectorGetX(XMMatrixDeterminant(
		static_cast<XMMATRIX>(m)));
}

std::wostream& operator<<(std::wostream& os, const Matrix& m) {
	return os <<
		m.r[0] << std::endl <<
		m.r[1] << std::endl <<
		m.r[2] << std::endl <<
		m.r[3];
}

// Quaternion

Quaternion::Quaternion(FXMVECTOR v) {
	XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(this), v);
}

Quaternion::Quaternion(const Vector3f& axis, const float angle) {
	*this = static_cast<Quaternion>(XMQuaternionRotationAxis(
		static_cast<XMVECTOR>(axis),
		angle));
}

Quaternion::Quaternion(const float yaw, const float pitch, const float roll) {
	*this = static_cast<Quaternion>(XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
}

Quaternion::Quaternion(const Matrix& m) {
	*this = static_cast<Quaternion>(XMQuaternionRotationMatrix(
		static_cast<XMMATRIX>(m)));
}

Quaternion::operator XMVECTOR() const {
	return XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(this));
}

Quaternion Quaternion::identity() {
	return {0, 0, 0, 1};
}

bool operator==(const Quaternion& lhs, const Quaternion& rhs) {
	return XMVector4Equal(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMVECTOR>(rhs));
}

bool operator!=(const Quaternion& lhs, const Quaternion& rhs) {
	return !(lhs == rhs);
}

bool operator<(const Quaternion& lhs, const Quaternion& rhs) {
	return std::lexicographical_compare(
		lhs.e.cbegin(), lhs.e.cend(),
		rhs.e.cbegin(), rhs.e.cend());
}

Quaternion operator+(const Quaternion& q) {
	return q;
}

Quaternion operator-(const Quaternion& q) {
	return static_cast<Quaternion>(XMVectorNegate(
		static_cast<XMVECTOR>(q)));
}

Quaternion& operator*=(Quaternion& lhs, const Quaternion& rhs) {
	return lhs = static_cast<Quaternion>(XMQuaternionMultiply(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMVECTOR>(rhs)));
}

Quaternion& operator*=(Quaternion& lhs, const float rhs) {
	return lhs = static_cast<Quaternion>(XMVectorScale(
		static_cast<XMVECTOR>(lhs), rhs));
}

Quaternion operator*(Quaternion lhs, const Quaternion& rhs) {
	return lhs *= rhs;
}

float length(const Quaternion& q) {
	return XMVectorGetX(XMQuaternionLength(static_cast<XMVECTOR>(q)));
}

float length2(const Quaternion& q) {
	return XMVectorGetX(XMQuaternionLengthSq(static_cast<XMVECTOR>(q)));
}

Quaternion normalize(const Quaternion& q) {
	return static_cast<Quaternion>(XMQuaternionNormalize(
		static_cast<XMVECTOR>(q)));
}

Quaternion conjugate(const Quaternion& q) {
	return static_cast<Quaternion>(XMQuaternionConjugate(
		static_cast<XMVECTOR>(q)));
}

Quaternion inverse(const Quaternion& q) {
	return static_cast<Quaternion>(XMQuaternionInverse(
		static_cast<XMVECTOR>(q)));
}

Quaternion slerp(const Quaternion& from, const Quaternion& to, const float t) {
	return static_cast<Quaternion>(XMQuaternionSlerp(
		static_cast<XMVECTOR>(from),
		static_cast<XMVECTOR>(to),
		t));
}

float dot(const Quaternion& lhs, const Quaternion& rhs) {
	return XMVectorGetX(XMQuaternionDot(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMVECTOR>(rhs)));
}

std::wostream& operator<<(std::wostream& os, const Quaternion& rhs) {
	return os << Vector4f{rhs.e[0], rhs.e[1], rhs.e[2], rhs.e[3]};
}

// Plane

Plane::Plane(const Point3f& p1, const Point3f& p2, const Point3f& p3) {
	assert(length2(p1-p2) != 0 && length2(p1-p3) != 0);
	*this = static_cast<Plane>(XMPlaneFromPoints(
		static_cast<XMVECTOR>(p1),
		static_cast<XMVECTOR>(p2),
		static_cast<XMVECTOR>(p3)));
}

Plane::Plane(const Point3f& point, const Vector3f& normal) {
	assert(length2(normal) != 0);
	*this = static_cast<Plane>(XMPlaneFromPointNormal(
		static_cast<XMVECTOR>(point),
		static_cast<XMVECTOR>(normal)));
}

Plane::Plane(FXMVECTOR v) {
	XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(this), v);
}

Plane::operator XMVECTOR() const {
	return XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(this));
}

Vector3f Plane::normal() const {
	return {a, b, c};
}

Point3f Plane::point() const {
	assert(a != 0 || b != 0 || c != 0);

	if (abs(a) > abs(b)) {
		if (abs(a) > abs(c))
			return {-d/a, 0, 0};
		else
			return {0, 0, -d/c};
	} else {
		if (abs(b) > abs(c))
			return {0, -d/b, 0};
		else
			return {0, 0, -d/c};
	}
}

bool operator==(const Plane& lhs, const Plane& rhs) {
	return XMVector4Equal(XMVECTOR(lhs), XMVECTOR(rhs));
}

bool operator!=(const Plane& lhs, const Plane& rhs) {
	return !(lhs == rhs);
}

bool operator<(const Plane& lhs, const Plane& rhs) {
	return std::lexicographical_compare(
		lhs.e.cbegin(), lhs.e.cend(),
		rhs.e.cbegin(), rhs.e.cend());
}

Plane& operator*=(Plane& lhs, const Matrix& rhs) {
	return lhs = static_cast<Plane>(XMPlaneTransform(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMMATRIX>(rhs)));
}

Plane& operator*=(Plane& lhs, const Quaternion& rhs) {
	return lhs = static_cast<Plane>(XMPlaneTransform(
		static_cast<XMVECTOR>(lhs),
		XMMatrixRotationQuaternion(static_cast<XMVECTOR>(rhs))));
}

Plane operator*(Plane lhs, const Matrix& rhs) {
	return lhs *= rhs;
}

Plane operator*(Plane lhs, const Quaternion& rhs) {
	return lhs *= rhs;
}

std::wostream& operator<<(std::wostream& os, const Plane& rhs) {
	return os << Vector4f{rhs.e[0], rhs.e[1], rhs.e[2], rhs.e[3]};
}

// Colour

Colour::Colour(FXMVECTOR v) {
	XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(this), v);
}

Colour::operator XMVECTOR() const {
	return XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(this));
}

const float* Colour::rgba_ptr() const {
	return &r;
}

PackedVector::XMCOLOR Colour::bgra() const {
	PackedVector::XMCOLOR pv;
	PackedVector::XMStoreColor(&pv, XMVECTOR(*this));
	return pv;
}

PackedVector::XMUBYTEN4 Colour::rgba() const {
	PackedVector::XMUBYTEN4 pv;
	PackedVector::XMStoreUByteN4(&pv, XMVECTOR(*this));
	return pv;
}

D2D1_COLOR_F Colour::d2d() const {
	return D2D1::ColorF{r, g, b, a};
}

bool operator==(const Colour& lhs, const Colour& rhs) {
	return XMVector4Equal(XMVECTOR(lhs), XMVECTOR(rhs));
}

bool operator!=(const Colour& lhs, const Colour& rhs) {
	return !(lhs == rhs);
}

bool operator<(const Colour& lhs, const Colour& rhs) {
	return std::lexicographical_compare(
		lhs.e.cbegin(), lhs.e.cend(),
		rhs.e.cbegin(), rhs.e.cend());
}

Colour& operator*=(Colour& lhs, const Colour& rhs) {
	return lhs = static_cast<Colour>(XMVectorMultiply(
		static_cast<XMVECTOR>(lhs),
		static_cast<XMVECTOR>(rhs)));
}

Colour& operator*=(Colour& lhs, const float rhs) {
	return lhs = static_cast<Colour>(XMVectorScale(
		static_cast<XMVECTOR>(lhs),
		rhs));
}

Colour& operator/=(Colour& lhs, const float rhs) {
	assert(rhs != 0);
	return lhs *= 1/rhs;
}

Colour operator*(Colour lhs, const Colour& rhs) {
	return lhs *= rhs;
}

Colour operator*(Colour lhs, const float rhs) {
	return lhs *= rhs;
}

Colour operator*(const float lhs, Colour rhs) {
	return rhs *= lhs;
}

Colour operator/(Colour lhs, const float rhs) {
	return lhs /= rhs;
}

std::wostream& operator<<(std::wostream& os, const Colour& rhs) {
	return os << Vector4f{rhs.e[0], rhs.e[1], rhs.e[2], rhs.e[3]};
}
