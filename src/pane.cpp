#include "shared.h"

#include "pane.h"

#include "d2d.h"
#include "edge.h"
#include "image.h"
#include "window.h"

#include "shared/map.h"
#include "shared/numeric_cast.h"

Pane::Pane(
	Window* const window,
	Edge* const edge_left,
	Edge* const edge_top,
	Edge* const edge_right,
	Edge* const edge_bottom,
	const D2D1_RECT_F margin,
	const bool fixed_width,
	const bool fixed_height,
	const Colour colour
) {
	this->window = window;

	width = 0;
	height = 0;
	this->fixed_width = fixed_width;
	this->fixed_height = fixed_height;

	this->margin = margin;
	this->colour = colour;
	cursor = er = LoadCursor(nullptr, IDC_ARROW);

	text_tooltip_window = nullptr;
	checkbox = nullptr;
	progressbar = nullptr;
	image_centre = {0.5f, 0.5f};
	image_scale = 1;

	this->edge_left = edge_left;
	this->edge_top = edge_top;
	this->edge_right = edge_right;
	this->edge_bottom = edge_bottom;

	er = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&dw_factory));
			
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof NONCLIENTMETRICS;
	er = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	float font_height_dip = window->to_dip_y(std::abs(ncm.lfMessageFont.lfHeight));
	er = dw_factory->CreateTextFormat(
		ncm.lfMessageFont.lfFaceName,
		nullptr,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		font_height_dip,
		L"en-us",
		&text_format);

	DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, L'\\', 2};
	ComPtr<IDWriteInlineObject> trimming_sign;
	er = dw_factory->CreateEllipsisTrimmingSign(text_format, &trimming_sign);
	er = text_format->SetTrimming(&trimming, trimming_sign);
	er = text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
}

Pane::~Pane() {
	text_format = nullptr;
	dw_factory = nullptr;

	if (button_font) {
		er = DeleteObject(button_font);
		button_font = nullptr;
	}

	for (auto& button : buttons)
		er = PostMessage(button, WM_CLOSE, 0, 0);

	if (checkbox)
		er = PostMessage(checkbox, WM_CLOSE, 0, 0);;

	if (text_tooltip_window)
		er = DestroyWindow(text_tooltip_window);

	if (progressbar) {
		er = KillTimer(window->get_handle(), progressbar_timer_id);
		er = PostMessage(progressbar, WM_CLOSE, 0, 0);
		er = progressbar_taskbar_list->SetProgressState(window->get_handle(), TBPF_NOPROGRESS);
	}
}

Pane::Pane(Pane&& rhs) {
	edge_left = rhs.edge_left;
	edge_top = rhs.edge_top;
	edge_right = rhs.edge_right;
	edge_bottom = rhs.edge_bottom;

	dw_factory = rhs.dw_factory;
	text_format = rhs.text_format;

	button_font = rhs.button_font;

	window = rhs.window;

	width = rhs.width;
	height = rhs.height;
	fixed_width = rhs.fixed_width;
	fixed_height = rhs.fixed_height;

	margin = rhs.margin;
	colour = rhs.colour;
	cursor = rhs.cursor;

	// text
	text = rhs.text;
	text_bold_ranges = rhs.text_bold_ranges;
	text_centred = rhs.text_centred;
	text_layout = rhs.text_layout;
	text_tooltip_window = rhs.text_tooltip_window;
	text_tooltip = rhs.text_tooltip;

	// button
	buttons = rhs.buttons;
	button_size = rhs.button_size;
	button_stride = rhs.button_stride;

	// checkbox
	checkbox = rhs.checkbox;

	// progressbar
	progressbar = rhs.progressbar;
	progressbar_mode = rhs.progressbar_mode;
	progressbar_taskbar_list = rhs.progressbar_taskbar_list;

	// image
	image = rhs.image;
	image_centre = rhs.image_centre;
	image_scale = rhs.image_scale;

	rhs.button_font = nullptr;
	rhs.buttons.clear();
	rhs.checkbox = nullptr;
	rhs.text_tooltip_window = nullptr;
	rhs.progressbar = nullptr;
}

bool Pane::has_width() const {
	return fixed_width;
}

bool Pane::has_height() const {
	return fixed_height;
}

float Pane::get_width() const {
	assert(fixed_width);
	return width;
}

float Pane::get_height() const {
	assert(fixed_height);
	return height;
}

D2D1_RECT_F Pane::container() const {
	return {
		edge_left->get_position(window->get_size().w),
		edge_top->get_position(window->get_size().h),
		edge_right->get_position(window->get_size().w),
		edge_bottom->get_position(window->get_size().h)};
}

D2D1_RECT_F Pane::content() const {
	return {
		margin.left + edge_left->get_position(window->get_size().w),
		margin.top + edge_top->get_position(window->get_size().h),
		edge_right->get_position(window->get_size().w) - margin.right,
		edge_bottom->get_position(window->get_size().h) - margin.bottom};
}

bool Pane::is_inside(const Point2f& position) const {
	return
		position.x >= container().left &&
		position.x <= container().right &&
		position.y >= container().top &&
		position.y <= container().bottom;
}

HCURSOR Pane::get_cursor() const {
	return cursor;
}

void Pane::set_cursor(LPCTSTR cursor_name) {
	cursor = er = LoadCursor(nullptr, cursor_name);
}

void Pane::update() {
	auto x = content().left;
	x += (rect_size(content()).w - buttons.size() * button_stride) / 2; // TODO: should be width from first button left edge to last button right edge
	for (auto& button : buttons) {
		er = SetWindowPos(
			button, nullptr,
			window->to_dp_x(x),
			window->to_dp_y(content().top),
			0, 0, SWP_NOCOPYBITS | SWP_NOSIZE | SWP_NOZORDER);
		x += button_stride;
	}

	if (progressbar) {
		auto progressbar_size = rect_size(get_client_rect(progressbar, window->get_scale()));

		er = SetWindowPos(
			progressbar, nullptr,
			window->to_dp_x(content().left + (rect_size(content()).w - progressbar_size.w) / 2),
			window->to_dp_y(content().top + (rect_size(content()).h - progressbar_size.h) / 2),
			0, 0, SWP_NOCOPYBITS | SWP_NOSIZE | SWP_NOZORDER);
	}

	if (text_tooltip_window) {
		er = DestroyWindow(text_tooltip_window);
		text_tooltip_window = nullptr;
	}

	if (!text.empty()) {
		if (content().right - content().left > 0 && content().bottom - content().top > 0) {
			er = dw_factory->CreateTextLayout(
				text.c_str(),
				numeric_cast<UINT32>(text.length()),
				text_format,
				content().right - content().left,
				content().bottom - content().top,
				&text_layout);

			for (const auto& range : text_bold_ranges) {
				DWRITE_TEXT_RANGE dtr = {
					numeric_cast<UINT32>(range.first),
					numeric_cast<UINT32>(range.second)
				};
				er = text_layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, dtr);
			}

			// any lines trimmed?
			BOOL is_trimmed = false;
			DWRITE_LINE_METRICS lm[8];
			UINT32 lines;
			er = text_layout->GetLineMetrics(lm, sizeof lm / sizeof DWRITE_LINE_METRICS, &lines);
			for (UINT32 i = 0; i < lines && !is_trimmed; i++)
				is_trimmed |= lm[i].isTrimmed;

			// add tooltip?
			if (is_trimmed) {
				text_tooltip_window = er = CreateWindowEx(
					WS_EX_TOOLWINDOW | WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_NOPREFIX,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
					window->get_handle(), nullptr, er = GetModuleHandle(nullptr), nullptr);

				// need to copy text to a string that is only modified here
				// since its data is used directly and not copied by the tooltip
				text_tooltip = text;

				TOOLINFO ti{
					sizeof TOOLINFO, TTF_SUBCLASS, window->get_handle(), 0,
					{window->to_dp_x(content().left), window->to_dp_y(content().top), window->to_dp_x(content().right), window->to_dp_y(content().bottom)},
					er = GetModuleHandle(nullptr), const_cast<LPWSTR>(text_tooltip.c_str()), 0, nullptr};
				SendMessage(text_tooltip_window, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
				SendMessage(text_tooltip_window, TTM_SETMAXTIPWIDTH, 0, numeric_cast<LPARAM>(window->get_size().w));
			}
		}
	}
}

/* transform a
 * normalised centre coordinate in image space to a
 * offset from top left offset in image space
 */
static Point2f centre_isn_to_offset_is(
	const Point2f& centre,
	const Size2f& bitmap_size,
	const Size2f& pane_size,
	const float scale
) {
	auto centre_is = Point2f{
		centre.x * bitmap_size.w,
		centre.y * bitmap_size.h};
	auto centre_ss = Point2f{
		centre_is.x * scale,
		centre_is.y * scale};
	auto offset_ss = Point2f{
		centre_ss.x - pane_size.w / 2.0f,
		centre_ss.y - pane_size.h / 2.0f};
	auto offset_is = Point2f{
		offset_ss.x / scale,
		offset_ss.y / scale};
	return offset_is;
}

/* is, image space (0 to width-1, 0 to height-1)
 * isn, normalised image space (0 to 1, 0 to 1)
 * ss, screen space (as image space but scaled)
 */

static Size2f get_source_rect_size(
	const Size2f& bitmap_size,
	const Size2f& pane_size,
	const float scale
) {
	// calculate width/height of source rectangle,
	// correcting for aspect ratio of destination rectangle
	auto width = bitmap_size.w * scale;
	auto height = bitmap_size.h * scale;
	auto width_new = std::min(width, pane_size.w);
	auto height_new = std::min(height, pane_size.h);
	auto scale_width = width_new / width;
	auto scale_height = height_new / height;
	auto bitmap_width = bitmap_size.w * scale_width;
	auto bitmap_height = bitmap_size.h * scale_height;

	return {bitmap_width, bitmap_height};
}

static D2D1_RECT_F get_source_rect(
	const Point2f& centre,
	const Size2f& bitmap_size,
	const Size2f& pane_size,
	const float scale
) {
	auto offset_is = centre_isn_to_offset_is(
		centre, bitmap_size, pane_size, scale);

	auto source_rect_size = get_source_rect_size(bitmap_size, pane_size, scale);

	// clamp offset to top left corner and
	// bottom right corner (minus width or height)
	offset_is.x = clamp(
		offset_is.x,
		0.0f,
		bitmap_size.w - source_rect_size.w);
	offset_is.y = clamp(
		offset_is.y,
		0.0f,
		bitmap_size.h - source_rect_size.h);

	return {
		offset_is.x,
		offset_is.y,
		offset_is.x + source_rect_size.w,
		offset_is.y + source_rect_size.h};
}

static D2D1_RECT_F get_destination_rect(
	const Size2f& bitmap_size,
	const D2D1_RECT_F& pane_rect,
	const float scale
) {
	// make rectangle as large as scaled bitmap
	auto width = bitmap_size.w * scale;
	auto height = bitmap_size.h * scale;

	// crop rectangle to pane size
	auto width_max = pane_rect.right - pane_rect.left;
	auto height_max = pane_rect.bottom - pane_rect.top;
	width = std::min(width, width_max);
	height = std::min(height, height_max);

	// centre rectangle in pane
	auto left = pane_rect.left + (width_max - width) / 2.0f;
	auto top = pane_rect.top + (height_max - height) / 2.0f;

	return {left, top, left + width, top + height};
}

void Pane::draw(ComPtr<ID2D1HwndRenderTarget> render_target) const {
	CoInitialize(nullptr);

	ComPtr<ID2D1SolidColorBrush> brush;

	er = render_target->CreateSolidColorBrush(colour.d2d(), &brush);
	render_target->FillRectangle(container(), brush);

	if (text_layout) {
		er = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush);

		Size2f padding{0, 0};

		padding.v = (rect_size(container()).h - height) / 2;
		padding.v = std::max(0.0f, padding.v);

		if (text_centred) {
			DWRITE_TEXT_METRICS tm;
			er = text_layout->GetMetrics(&tm);
			padding.h = (rect_size(content()).w - tm.width) / 2;
			padding.h = std::max(0.0f, padding.h);
		}

		render_target->DrawTextLayout(
			D2D1::Point2F(content().left + padding.h, content().top + padding.v),
			text_layout,
			brush);
	}

	if (image)
		image->draw(
			render_target,
			get_destination_rect(
				image->get_bitmap_size(window->get_scale()),
				content(),
				image_scale),
			get_source_rect(
				image_centre,
				image->get_bitmap_size(window->get_scale()),
				rect_size(content()),
				image_scale),
			image_scale < 1.0f ? D2D1_BITMAP_INTERPOLATION_MODE_LINEAR : D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);

	#if 0
	er = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &brush);
	render_target->DrawRectangle(content(), brush);
	er = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), &brush);
	render_target->DrawRectangle(container(), brush);
	#endif

	CoUninitialize();
}

void Pane::set_text(
	const std::wstring& text,
	const std::vector<std::pair<std::size_t, std::size_t>>& bold_ranges,
	bool centred
) {
	this->text = text;
	this->text_bold_ranges = bold_ranges;
	this->text_centred = centred;

	if (fixed_width || fixed_height) {
		// determine maximum text width and height
		er = dw_factory->CreateTextLayout(
			text.c_str(),
			numeric_cast<UINT32>(text.length()),
			text_format,
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max(),
			&text_layout);

		DWRITE_TEXT_METRICS tm;
		er = text_layout->GetMetrics(&tm);

		auto width_new = margin.left + ceil(tm.width) + margin.right;
		auto height_new = margin.top + ceil(tm.height) + margin.bottom;

		bool pane_size_change =
			(fixed_width && width_new != width) ||
			(fixed_height && height_new != height);
		window->layout_valid = !pane_size_change;

		if (fixed_width)
			width = margin.left + ceil(tm.width) + margin.right;
		if (fixed_height)
			height = margin.top + ceil(tm.height) + margin.bottom;
	}

	window->layout_valid = false;
	window->set_dirty();
}

void Pane::set_progressbar_progress(const float progress) {
	if (!progressbar) {
		//int width = rect.right - rect.left - 2*margin;
		//int height = er = GetSystemMetrics(SM_CYVSCROLL);
		const Size2f ms_recommended_progressbar_size = {355, 15};

		progressbar = er = CreateWindowEx(
			0, PROGRESS_CLASS, nullptr, WS_CHILD | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT,
			window->to_dp_x(ms_recommended_progressbar_size.w),
			window->to_dp_y(ms_recommended_progressbar_size.h),
			window->get_handle(), 0, er = GetModuleHandle(nullptr), nullptr);

		er = CoCreateInstance(
			CLSID_TaskbarList, nullptr, CLSCTX_ALL,
			IID_ITaskbarList3, reinterpret_cast<void**>(&progressbar_taskbar_list));

		er = SetTimer(window->get_handle(), progressbar_timer_id, progressbar_timer_ms, nullptr);

		progressbar_mode = ProgressbarMode::unknown;

		width = ms_recommended_progressbar_size.w;
		height = ms_recommended_progressbar_size.h;

		window->layout_valid = false;
	}

	if (progress < 0.0f || progress > 1.0f) {
		if (progressbar_mode != ProgressbarMode::indeterminate) {
			er = SetWindowLongPtr(progressbar, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_MARQUEE);
			er = PostMessage(progressbar, PBM_SETMARQUEE, true, 0);
			er = progressbar_taskbar_list->SetProgressState(window->get_handle(), TBPF_INDETERMINATE);
			progressbar_mode = ProgressbarMode::indeterminate;
		}
	} else {
		if (progressbar_mode != ProgressbarMode::normal) {
			er = SetWindowLongPtr(progressbar, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);
			er = PostMessage(progressbar, PBM_SETMARQUEE, false, 0);
			er = progressbar_taskbar_list->SetProgressState(window->get_handle(), TBPF_NORMAL);
			progressbar_mode = ProgressbarMode::normal;
		}

		auto max_value = std::numeric_limits<std::int16_t>::max();
		auto value = std::min(static_cast<decltype(max_value)>(progress * max_value), max_value);
		er = PostMessage(progressbar, PBM_SETRANGE32, 0, max_value);
		er = PostMessage(progressbar, PBM_SETPOS, value, 0);
		er = progressbar_taskbar_list->SetProgressValue(window->get_handle(), value, max_value);
	}
}

static LRESULT WINAPI button_window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_KEYDOWN)
		SendMessage(GetParent(hwnd), msg, wparam, lparam);
	auto wp = er = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	return CallWindowProc(wp, hwnd, msg, wparam, lparam);
}

void Pane::add_button(const int button_id, const std::wstring& label) {
	auto bw = er = CreateWindowEx(
		0, WC_BUTTON, label.c_str(), WS_CHILD | WS_TABSTOP | WS_VISIBLE | BS_PUSHBUTTON,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		window->get_handle(), reinterpret_cast<HMENU>(static_cast<std::intptr_t>(button_id)),
		er = GetModuleHandle(nullptr), nullptr);
	buttons.push_back(bw);

	// save old window procedure in button window's userdata field and set new window procedure
	auto wp = er = GetWindowLongPtr(bw, GWLP_WNDPROC);
	er = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == 0; // 0 is default value AND error code
	er = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == wp;
	er = SetWindowLongPtr(bw, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(button_window_procedure));

	// button font
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof NONCLIENTMETRICS;
	er = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	// reported font height is in DP but increases with windows DPI settings.
	// revert this increase to get the font height in DIP.
	auto font_height_dip = window->to_dip_y(std::abs(ncm.lfMessageFont.lfHeight));
	if (button_font == nullptr) {
		ncm.lfMessageFont.lfHeight = -window->to_dp_y(font_height_dip);
		button_font = er = CreateFontIndirect(&ncm.lfMessageFont);
	}
	SendMessage(bw, WM_SETFONT, reinterpret_cast<WPARAM>(button_font), MAKELPARAM(true, 0));

	// button size

	const auto button_margin = 8.0f;
	const auto button_vertical_size_margin = 1.0f;

	auto size_max = Size2f{0, 0};
	for (auto& button : buttons) {
		SIZE size_dp;
		er = Button_GetIdealSize(button, &size_dp);
		size_max.w = std::max(window->to_dip_x(size_dp.cx), size_max.w);
		size_max.h = std::max(window->to_dip_x(size_dp.cy), size_max.h);
	}

	auto button_size = Size2f{
		size_max.w + font_height_dip,
		size_max.h + 2*button_vertical_size_margin};
	button_size.w = std::max(button_size.w, 80.0f);

	for (auto& button : buttons)
		er = SetWindowPos(
			button, nullptr, 0, 0,
			window->to_dp_x(button_size.w),
			window->to_dp_y(button_size.h),
			SWP_NOMOVE | SWP_NOZORDER);

	button_stride = button_size.w + button_margin;

	width = button_size.w * buttons.size();
	width += button_margin * (buttons.size() - 1);
	width += margin.left + margin.right;

	height = margin.top + button_size.h + margin.bottom;

	window->layout_valid = false;
}

void Pane::add_combobox(const int button_id, const std::vector<std::wstring>& items) {
	auto bw = er = CreateWindowEx(
		0, WC_COMBOBOX, L"", WS_CHILD | WS_TABSTOP | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		window->get_handle(), reinterpret_cast<HMENU>(static_cast<std::intptr_t>(button_id)),
		er = GetModuleHandle(nullptr), nullptr);
	buttons.push_back(bw);

	for (auto& item : items)
		SendMessage(bw, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
	SendMessage(bw, CB_SETCURSEL, 0, 0);

	// save old window procedure in button window's userdata field and set new window procedure
	auto wp = er = GetWindowLongPtr(bw, GWLP_WNDPROC);
	er = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == 0; // 0 is default value AND error code
	er = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == wp;
	er = SetWindowLongPtr(bw, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(button_window_procedure));

	// button font
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof NONCLIENTMETRICS;
	er = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	// reported font height is in DP but increases with windows DPI settings.
	// revert this increase to get the font height in DIP.
	float font_height_dip = window->to_dip_y(std::abs(ncm.lfMessageFont.lfHeight));
	if (button_font == nullptr) {
		ncm.lfMessageFont.lfHeight = -window->to_dp_y(font_height_dip);
		button_font = er = CreateFontIndirect(&ncm.lfMessageFont);
	}
	SendMessage(bw, WM_SETFONT, reinterpret_cast<WPARAM>(button_font), MAKELPARAM(true, 0));

	// button size

	const auto button_margin = 8.0f;
	const auto button_vertical_size_margin = 1.0f;

	auto size_max = Size2f{0, 0};
	for (auto& button : buttons) {
		SIZE size_dp;
		er = Button_GetIdealSize(button, &size_dp);
		size_max.w = std::max(window->to_dip_x(size_dp.cx), size_max.w);
		size_max.h = std::max(window->to_dip_x(size_dp.cy), size_max.h);
	}

	auto button_size = Size2f{
		size_max.w + font_height_dip,
		size_max.h + 2*button_vertical_size_margin};

	for (auto& button : buttons)
		er = SetWindowPos(
			button, nullptr, 0, 0,
			window->to_dp_x(button_size.w),
			window->to_dp_y(button_size.h),
			SWP_NOMOVE | SWP_NOZORDER);

	button_stride = button_size.w + button_margin;

	width = button_size.w * buttons.size();
	width += button_margin * (buttons.size() - 1);
	width += margin.left + margin.right;

	height = margin.top + button_size.h + margin.bottom;

	window->layout_valid = false;
}

std::shared_ptr<Image> Pane::get_image() const {
	return image;
}

void Pane::set_image(const std::shared_ptr<Image> image) {
	this->image = image;
}

float Pane::get_image_scale() const {
	return image_scale;
}

void Pane::set_image_scale(const float scale) {
	image_scale = scale;
}

static Point2f clamp_centre(
	const Size2f& pane_size,
	const Size2f& bitmap_size,
	const float scale,
	const Point2f& centre
) {
	// calculate margins for both images and clamp centre to these.
	// returned centre position should be inside the margins of both images.

	// margin is the minimum distance from the edges of
	// rectangle (0, 0) -> (1, 1) that centre may be.
	// normalised margin size is
	// (half the pane size) / (entire image size adjusted by scale)

	auto margin = Size2f{
		(pane_size.w / 2.0f) / (bitmap_size.w * scale),
		(pane_size.h / 2.0f) / (bitmap_size.h * scale)};
	margin = {
		std::min(margin.w, 0.5f),
		std::min(margin.h, 0.5f)};

	auto centre_min = Point2f{margin.w, margin.h};
	auto centre_max = Point2f{1.0f - margin.w, 1.0f - margin.h};
	return {
		clamp(centre.x, centre_min.x, centre_max.x),
		clamp(centre.y, centre_min.y, centre_max.y)};
}

void Pane::image_zoom_transform(
	const float scale,
	const Point2f& zoom_point_ss,
	const Vector2f& dpi_scale
) {
	// zoom point is position relative to centre of content rect

	// if images have different dimensions, centre may not indicate
	// the actual the centre point in both panes. this happens near the
	// image edges, where actual image positions in the panes are clamped.

	// to transform the centre in old scale to centre in new scale, we need
	// the actual centre in the active pane.

	auto bitmap_size = image->get_bitmap_size(dpi_scale);

	// transform centre (normalized image space) to offset (image space)
	auto offset_is = centre_isn_to_offset_is(
		image_centre,
		bitmap_size,
		rect_size(content()),
		image_scale);

	// clamp offset to image (taking pane size into account)
	auto offset_max_is = Point2f{
		std::max(0.0f, bitmap_size.w - rect_size(content()).w / image_scale),
		std::max(0.0f, bitmap_size.h - rect_size(content()).h / image_scale)};
	offset_is = {
		clamp(offset_is.x, 0.0f, offset_max_is.x),
		clamp(offset_is.y, 0.0f, offset_max_is.y)};

	// calculate actual centre
	// using pane size OR image size (whichever is smaller)
	auto image_extent = Size2f{
		std::min(rect_size(content()).w, bitmap_size.w * image_scale),
		std::min(rect_size(content()).h, bitmap_size.h * image_scale)};
	auto centre_ss = Point2f{
		offset_is.x*image_scale + image_extent.w/2.0f,
		offset_is.y*image_scale + image_extent.h/2.0f};
	image_centre = {
			centre_ss.x / image_scale / bitmap_size.w,
			centre_ss.y / image_scale / bitmap_size.h};

	// transform centre in old scale to centre in new scale
	image_centre = {
		image_centre.x + zoom_point_ss.x / bitmap_size.w * (1.0f / image_scale - 1.0f / scale),
		image_centre.y + zoom_point_ss.y / bitmap_size.h * (1.0f / image_scale - 1.0f / scale)};

	image_scale = scale;

	image_centre = clamp_centre(rect_size(content()), image->get_bitmap_size(dpi_scale), image_scale, image_centre);
}

void Pane::set_image_centre_from_other_pane(
	const Pane& pane_other,
	const Vector2f& dpi_scale
) {
	auto margin_other = Size2f{
		rect_size(pane_other.content()).w / 2,
		rect_size(pane_other.content()).h / 2};
	auto margin_this = Size2f{
		rect_size(content()).w / 2,
		rect_size(content()).h / 2};

	auto bitmap_size = image->get_bitmap_size(dpi_scale);
	auto bitmap_size_other = pane_other.image->get_bitmap_size(dpi_scale);

	auto centre_other_ss = Point2f{
		pane_other.image_centre.x * bitmap_size_other.w * pane_other.image_scale,
		pane_other.image_centre.y * bitmap_size_other.h * pane_other.image_scale};

	auto panning_freedom_other = Size2f{
		std::max(0.0f, bitmap_size_other.w * pane_other.image_scale - 2 * margin_other.w),
		std::max(0.0f, bitmap_size_other.h * pane_other.image_scale - 2 * margin_other.h)};
	auto panning_freedom_this = Size2f{
		std::max(0.0f, bitmap_size.w * image_scale - 2 * margin_this.w),
		std::max(0.0f, bitmap_size.h * image_scale - 2 * margin_this.h)};

	// if other image cannot be panned, don't bother copying its
	// centre since it will always be placed at (0.5, 0.5)
	if (panning_freedom_other.w == 0 && panning_freedom_other.h == 0)
		return;

	auto panning_normalized = Point2f{
		panning_freedom_other.w == 0 ? 0.5f : (centre_other_ss.x - margin_other.w) / panning_freedom_other.w,
		panning_freedom_other.h == 0 ? 0.5f : (centre_other_ss.y - margin_other.h) / panning_freedom_other.h};

	auto centre_this_ss = Point2f{
		panning_normalized.x * panning_freedom_this.w + margin_this.w,
		panning_normalized.y * panning_freedom_this.h + margin_this.h};

	image_centre = {
		centre_this_ss.x / bitmap_size.w / image_scale,
		centre_this_ss.y / bitmap_size.h / image_scale};

	image_centre = clamp_centre(rect_size(content()), image->get_bitmap_size(dpi_scale), image_scale, image_centre);
}

void Pane::translate_image_centre(const Vector2f& translation_isn, const Vector2f& dpi_scale) {
	image_centre += translation_isn;
	image_centre = clamp_centre(rect_size(content()), image->get_bitmap_size(dpi_scale), image_scale, image_centre);
}
