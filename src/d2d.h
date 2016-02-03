#pragma once

#include <ostream>

#include <d2d1.h>

D2D1_SIZE_F rect_size(const D2D1_RECT_F& rect);
D2D1_RECT_F get_client_rect(const HWND hwnd, const D2D1_POINT_2F& scale);

D2D1_POINT_2F& operator+=(D2D1_POINT_2F& lhs, const D2D1_POINT_2F& rhs);

std::wostream& operator<<(std::wostream& os, const D2D1_POINT_2F& point);
std::wostream& operator<<(std::wostream& os, const D2D1_SIZE_F& size);
std::wostream& operator<<(std::wostream& os, const D2D1_SIZE_U& size);
