#include "shared.h"

#include "d2d.h"

Size2f rect_size(const D2D1_RECT_F& rect) {
	return {rect.right - rect.left, rect.bottom - rect.top};
}

D2D1_RECT_F get_client_rect(const HWND hwnd, const Vector2f& scale) {
	RECT r;
	er = GetClientRect(hwnd, &r);
	return {r.left / scale.x, r.top / scale.y, r.right / scale.x, r.bottom / scale.y};
}
