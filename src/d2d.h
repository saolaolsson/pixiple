#pragma once

#include "shared/vector.h"

#include <d2d1.h>

Size2f rect_size(const D2D1_RECT_F& rect);
D2D1_RECT_F get_client_rect(const HWND hwnd, const Vector2f& scale);
