#include "shared.h"

#include "d2d.h"

D2D1_SIZE_F rect_size(const D2D1_RECT_F& rect) {
	return { rect.right - rect.left, rect.bottom - rect.top };
}

D2D1_RECT_F get_client_rect(const HWND hwnd, const D2D1_POINT_2F& scale) {
	RECT r;
	et = GetClientRect(hwnd, &r);
	return { r.left / scale.x, r.top / scale.y, r.right / scale.x, r.bottom / scale.y };
}

D2D1_POINT_2F& operator+=(D2D1_POINT_2F& lhs, const D2D1_POINT_2F& rhs) {
	lhs.x += rhs.x;
	lhs.y += rhs.y;
	return lhs;
}

std::wostream& operator<<(std::wostream& os, const D2D1_POINT_2F& point) {
	os << L"(" << point.x << L", " << point.y << L")";
	return os;
}

std::wostream& operator<<(std::wostream& os, const D2D1_SIZE_F& size) {
	os << L"(" << size.width << L", " << size.height << L")";
	return os;
}

std::wostream& operator<<(std::wostream& os, const D2D1_SIZE_U& size) {
	os << L"(" << size.width << L", " << size.height << L")";
	return os;
}
