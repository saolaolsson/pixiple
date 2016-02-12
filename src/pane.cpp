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
	const D2D1_COLOR_F colour
) {
	this->window = window;

	width = 0;
	height = 0;
	this->fixed_width = fixed_width;
	this->fixed_height = fixed_height;

	this->margin = margin;
	this->colour = colour;
	cursor = et = LoadCursor(nullptr, IDC_ARROW);

	text_tooltip_window = nullptr;
	checkbox = nullptr;
	progressbar = nullptr;
	image_centre = {0.5f, 0.5f};
	image_scale = 1;

	this->edge_left = edge_left;
	this->edge_top = edge_top;
	this->edge_right = edge_right;
	this->edge_bottom = edge_bottom;

	et = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&dw_factory));
			
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof NONCLIENTMETRICS;
	et = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	float font_height_dip = window->to_dip_y(std::abs(ncm.lfMessageFont.lfHeight));
	et = dw_factory->CreateTextFormat(
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
	et = dw_factory->CreateEllipsisTrimmingSign(text_format, &trimming_sign);
	et = text_format->SetTrimming(&trimming, trimming_sign);
	et = text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
}

Pane::~Pane() {
	text_format = nullptr;
	dw_factory = nullptr;

	if (button_font) {
		et = DeleteObject(button_font);
		button_font = nullptr;
	}

	for (auto& button : buttons)
		et = PostMessage(button, WM_CLOSE, 0, 0);

	if (checkbox)
		et = PostMessage(checkbox, WM_CLOSE, 0, 0);;

	if (text_tooltip_window)
		et = DestroyWindow(text_tooltip_window);

	if (progressbar) {
		et = KillTimer(window->get_handle(), progressbar_timer_id);
		et = PostMessage(progressbar, WM_CLOSE, 0, 0);
		et = progressbar_taskbar_list->SetProgressState(window->get_handle(), TBPF_NOPROGRESS);
	}
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
		edge_left->get_position(window->get_size().width),
		edge_top->get_position(window->get_size().height),
		edge_right->get_position(window->get_size().width),
		edge_bottom->get_position(window->get_size().height)};
}

D2D1_RECT_F Pane::content() const {
	return {
		margin.left + edge_left->get_position(window->get_size().width),
		margin.top + edge_top->get_position(window->get_size().height),
		edge_right->get_position(window->get_size().width) - margin.right,
		edge_bottom->get_position(window->get_size().height) - margin.bottom};
}

bool Pane::is_inside(const D2D1_POINT_2F& position) const {
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
	cursor = et = LoadCursor(nullptr, cursor_name);
}

void Pane::update() {
	auto x = content().left;
	for (auto& button : buttons) {
		et = SetWindowPos(
			button, nullptr,
			window->to_dp_x(x),
			window->to_dp_y(content().top),
			0, 0, SWP_NOCOPYBITS | SWP_NOSIZE | SWP_NOZORDER);
		x += button_stride;
	}

	if (progressbar) {
		auto content_size = rect_size(content());
		auto progressbar_size = rect_size(get_client_rect(progressbar, window->get_scale()));

		et = SetWindowPos(
			progressbar, nullptr,
			window->to_dp_x((content_size.width - progressbar_size.width) / 2),
			window->to_dp_y((content_size.height - progressbar_size.height) / 2),
			0, 0, SWP_NOCOPYBITS | SWP_NOSIZE | SWP_NOZORDER);
	}

	if (text_tooltip_window) {
		et = DestroyWindow(text_tooltip_window);
		text_tooltip_window = nullptr;
	}

	if (!text.empty()) {
		if (content().right - content().left > 0 && content().bottom - content().top > 0) {
			et = dw_factory->CreateTextLayout(
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
				et = text_layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, dtr);
			}

			// any lines trimmed?
			BOOL is_trimmed = false;
			DWRITE_LINE_METRICS lm[8];
			UINT32 lines;
			et = text_layout->GetLineMetrics(lm, sizeof lm / sizeof DWRITE_LINE_METRICS, &lines);
			for (UINT32 i = 0; i < lines && !is_trimmed; i++)
				is_trimmed |= lm[i].isTrimmed;

			// add tooltip?
			if (is_trimmed) {
				text_tooltip_window = et = CreateWindowEx(
					WS_EX_TOOLWINDOW | WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_NOPREFIX,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
					window->get_handle(), nullptr, et = GetModuleHandle(nullptr), nullptr);

				// need to copy text to a string that is only modified here
				// since its data is used directly and not copied by the tooltip
				text_tooltip = text;

				TOOLINFO ti{
					sizeof TOOLINFO, TTF_SUBCLASS, window->get_handle(), 0,
					{window->to_dp_x(content().left), window->to_dp_y(content().top), window->to_dp_x(content().right), window->to_dp_y(content().bottom)},
					et = GetModuleHandle(nullptr), const_cast<LPWSTR>(text_tooltip.c_str()), 0, nullptr};
				SendMessage(text_tooltip_window, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
				SendMessage(text_tooltip_window, TTM_SETMAXTIPWIDTH, 0, numeric_cast<LPARAM>(window->get_size().width));
			}
		}
	}
}

/* transform a
 * normalised centre coordinate in image space to a
 * offset from top left offset in image space
 */
static D2D1_POINT_2F centre_isn_to_offset_is(
	const D2D1_POINT_2F& centre,
	const D2D1_SIZE_F& bitmap_size,
	const D2D1_SIZE_F& pane_size,
	const float scale
) {
	D2D1_POINT_2F centre_is{
		centre.x * bitmap_size.width,
		centre.y * bitmap_size.height};
	D2D1_POINT_2F centre_ss{
		centre_is.x * scale,
		centre_is.y * scale};
	D2D1_POINT_2F offset_ss{
		centre_ss.x - pane_size.width / 2.0f,
		centre_ss.y - pane_size.height / 2.0f};
	D2D1_POINT_2F offset_is{
		offset_ss.x / scale,
		offset_ss.y / scale};
	return offset_is;
}

/* is, image space (0 to width-1, 0 to height-1)
 * isn, normalised image space (0 to 1, 0 to 1)
 * ss, screen space (as image space but scaled)
 */

static D2D1_SIZE_F get_source_rect_size(
	const D2D1_SIZE_F& bitmap_size,
	const D2D1_SIZE_F& pane_size,
	const float scale
) {
	// calculate width/height of source rectangle,
	// correcting for aspect ratio of destination rectangle
	auto width = bitmap_size.width * scale;
	auto height = bitmap_size.height * scale;
	auto width_new = std::min(width, pane_size.width);
	auto height_new = std::min(height, pane_size.height);
	auto scale_width = width_new / width;
	auto scale_height = height_new / height;
	auto bitmap_width = bitmap_size.width * scale_width;
	auto bitmap_height = bitmap_size.height * scale_height;

	return {bitmap_width, bitmap_height};
}

static D2D1_RECT_F get_source_rect(
	const D2D1_POINT_2F& centre,
	const D2D1_SIZE_F& bitmap_size,
	const D2D1_SIZE_F& pane_size,
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
		bitmap_size.width - source_rect_size.width);
	offset_is.y = clamp(
		offset_is.y,
		0.0f,
		bitmap_size.height - source_rect_size.height);

	return {
		offset_is.x,
		offset_is.y,
		offset_is.x + source_rect_size.width,
		offset_is.y + source_rect_size.height};
}

static D2D1_RECT_F get_destination_rect(
	const D2D1_SIZE_F& bitmap_size,
	const D2D1_RECT_F& pane_rect,
	const float scale
) {
	// make rectangle as large as scaled bitmap
	auto width = bitmap_size.width * scale;
	auto height = bitmap_size.height * scale;

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
	ComPtr<ID2D1SolidColorBrush> brush;

	et = render_target->CreateSolidColorBrush(colour, &brush);
	render_target->FillRectangle(container(), brush);

	if (text_layout) {
		et = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush);

		auto padding = (rect_size(container()).height - height) / 2;
		padding = std::max(padding, 0.0f);

		render_target->DrawTextLayout(
			D2D1::Point2F(content().left, content().top + padding),
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
	et = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &brush);
	render_target->DrawRectangle(content(), brush);
	et = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), &brush);
	render_target->DrawRectangle(container(), brush);
	#endif
}

void Pane::set_text(
	const std::wstring& text,
	const std::vector<std::pair<std::size_t, std::size_t>>& bold_ranges
) {
	this->text = text;
	this->text_bold_ranges = bold_ranges;

	if (fixed_width || fixed_height) {
		// determine maximum text width and height
		et = dw_factory->CreateTextLayout(
			text.c_str(),
			numeric_cast<UINT32>(text.length()),
			text_format,
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max(),
			&text_layout);

		DWRITE_TEXT_METRICS tm;
		et = text_layout->GetMetrics(&tm);

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
}

void Pane::set_progressbar_progress(const float progress) {
	if (!progressbar) {
		//int width = rect.right - rect.left - 2*margin;
		//int height = et = GetSystemMetrics(SM_CYVSCROLL);
		const D2D1_SIZE_F ms_recommended_progressbar_size = {355, 15};

		progressbar = et = CreateWindowEx(
			0, PROGRESS_CLASS, nullptr, WS_CHILD | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT,
			window->to_dp_x(ms_recommended_progressbar_size.width),
			window->to_dp_y(ms_recommended_progressbar_size.height),
			window->get_handle(), 0, et = GetModuleHandle(nullptr), nullptr);

		et = CoCreateInstance(
			CLSID_TaskbarList, nullptr, CLSCTX_ALL,
			IID_ITaskbarList3, reinterpret_cast<void**>(&progressbar_taskbar_list));

		et = SetTimer(window->get_handle(), progressbar_timer_id, progressbar_timer_ms, nullptr);

		progressbar_mode = ProgressbarMode::unknown;

		window->layout_valid = false;
	}

	if (progress < 0.0f || progress > 1.0f) {
		if (progressbar_mode != ProgressbarMode::indeterminate) {
			et = SetWindowLongPtr(progressbar, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_MARQUEE);
			et = PostMessage(progressbar, PBM_SETMARQUEE, true, 0);
			et = progressbar_taskbar_list->SetProgressState(window->get_handle(), TBPF_INDETERMINATE);
			progressbar_mode = ProgressbarMode::indeterminate;
		}
	} else {
		if (progressbar_mode != ProgressbarMode::normal) {
			et = SetWindowLongPtr(progressbar, GWL_STYLE, WS_CHILD | WS_VISIBLE | PBS_SMOOTH);
			et = PostMessage(progressbar, PBM_SETMARQUEE, false, 0);
			et = progressbar_taskbar_list->SetProgressState(window->get_handle(), TBPF_NORMAL);
			progressbar_mode = ProgressbarMode::normal;
		}

		auto max_value = std::numeric_limits<std::int16_t>::max();
		auto value = std::min(static_cast<decltype(max_value)>(progress * max_value), max_value);
		et = PostMessage(progressbar, PBM_SETRANGE32, 0, max_value);
		et = PostMessage(progressbar, PBM_SETPOS, value, 0);
		et = progressbar_taskbar_list->SetProgressValue(window->get_handle(), value, max_value);
	}
}

static LRESULT WINAPI button_window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_KEYDOWN)
		SendMessage(GetParent(hwnd), msg, wparam, lparam);
	auto wp = et = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	return CallWindowProc(wp, hwnd, msg, wparam, lparam);
}

void Pane::add_button(const int button_id, const std::wstring& label) {
	auto bw = et = CreateWindowEx(
		0, WC_BUTTON, label.c_str(), WS_CHILD | WS_TABSTOP | WS_VISIBLE | BS_PUSHBUTTON,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		window->get_handle(), reinterpret_cast<HMENU>(static_cast<std::intptr_t>(button_id)),
		et = GetModuleHandle(nullptr), nullptr);
	buttons.push_back(bw);

	// save old window procedure in button window's userdata field and set new window procedure
	auto wp = et = GetWindowLongPtr(bw, GWLP_WNDPROC);
	et = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == 0; // 0 is default value AND error code
	et = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == wp;
	et = SetWindowLongPtr(bw, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(button_window_procedure));

	// button font
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof NONCLIENTMETRICS;
	et = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	// reported font height is in DP but increases with windows DPI settings.
	// revert this increase to get the font height in DIP.
	auto font_height_dip = window->to_dip_y(std::abs(ncm.lfMessageFont.lfHeight));
	if (button_font == nullptr) {
		ncm.lfMessageFont.lfHeight = -window->to_dp_y(font_height_dip);
		button_font = et = CreateFontIndirect(&ncm.lfMessageFont);
	}
	SendMessage(bw, WM_SETFONT, reinterpret_cast<WPARAM>(button_font), MAKELPARAM(true, 0));

	// button size

	const auto button_margin = 8.0f;
	const auto button_vertical_size_margin = 1.0f;

	auto size_max = D2D1::SizeF(0, 0);
	for (auto& button : buttons) {
		SIZE size_dp;
		et = Button_GetIdealSize(button, &size_dp);
		size_max.width = std::max(window->to_dip_x(size_dp.cx), size_max.width);
		size_max.height = std::max(window->to_dip_x(size_dp.cy), size_max.height);
	}

	auto button_size = D2D1::SizeF(
		size_max.width + font_height_dip,
		size_max.height + 2*button_vertical_size_margin);

	for (auto& button : buttons)
		et = SetWindowPos(
			button, nullptr, 0, 0,
			window->to_dp_x(button_size.width),
			window->to_dp_y(button_size.height),
			SWP_NOMOVE | SWP_NOZORDER);

	button_stride = button_size.width + button_margin;

	width = button_size.width * buttons.size();
	width += button_margin * (buttons.size() - 1);
	width += margin.left + margin.right;

	height = margin.top + button_size.height + margin.bottom;

	window->layout_valid = false;
}

void Pane::add_combobox(const int button_id, const std::vector<std::wstring>& items) {
	auto bw = et = CreateWindowEx(
		0, WC_COMBOBOX, L"", WS_CHILD | WS_TABSTOP | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		window->get_handle(), reinterpret_cast<HMENU>(static_cast<std::intptr_t>(button_id)),
		et = GetModuleHandle(nullptr), nullptr);
	buttons.push_back(bw);

	for (auto& item : items)
		SendMessage(bw, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
	SendMessage(bw, CB_SETCURSEL, 0, 0);

	// save old window procedure in button window's userdata field and set new window procedure
	auto wp = et = GetWindowLongPtr(bw, GWLP_WNDPROC);
	et = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == 0; // 0 is default value AND error code
	et = SetWindowLongPtr(bw, GWLP_USERDATA, wp) == wp;
	et = SetWindowLongPtr(bw, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(button_window_procedure));

	// button font
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof NONCLIENTMETRICS;
	et = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof NONCLIENTMETRICS, &ncm, 0);
	// reported font height is in DP but increases with windows DPI settings.
	// revert this increase to get the font height in DIP.
	float font_height_dip = window->to_dip_y(std::abs(ncm.lfMessageFont.lfHeight));
	if (button_font == nullptr) {
		ncm.lfMessageFont.lfHeight = -window->to_dp_y(font_height_dip);
		button_font = et = CreateFontIndirect(&ncm.lfMessageFont);
	}
	SendMessage(bw, WM_SETFONT, reinterpret_cast<WPARAM>(button_font), MAKELPARAM(true, 0));

	// button size

	const auto button_margin = 8.0f;
	const auto button_vertical_size_margin = 1.0f;

	auto size_max = D2D1::SizeF(0, 0);
	for (auto& button : buttons) {
		SIZE size_dp;
		et = Button_GetIdealSize(button, &size_dp);
		size_max.width = std::max(window->to_dip_x(size_dp.cx), size_max.width);
		size_max.height = std::max(window->to_dip_x(size_dp.cy), size_max.height);
	}

	auto button_size = D2D1::SizeF(
		size_max.width + font_height_dip,
		size_max.height + 2*button_vertical_size_margin);

	for (auto& button : buttons)
		et = SetWindowPos(
			button, nullptr, 0, 0,
			window->to_dp_x(button_size.width),
			window->to_dp_y(button_size.height),
			SWP_NOMOVE | SWP_NOZORDER);

	button_stride = button_size.width + button_margin;

	width = button_size.width * buttons.size();
	width += button_margin * (buttons.size() - 1);
	width += margin.left + margin.right;

	height = margin.top + button_size.height + margin.bottom;

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

static D2D1_POINT_2F clamp_centre(
	const D2D1_SIZE_F& pane_size,
	const D2D1_SIZE_F& bitmap_size,
	const float scale,
	const D2D1_POINT_2F& centre
) {
	// calculate margins for both images and clamp centre to these.
	// returned centre position should be inside the margins of both images.

	// margin is the minimum distance from the edges of
	// rectangle (0, 0) -> (1, 1) that centre may be.
	// normalised margin size is
	// (half the pane size) / (entire image size adjusted by scale)

	auto margin = D2D1::SizeF(
		(pane_size.width / 2.0f) / (bitmap_size.width * scale),
		(pane_size.height / 2.0f) / (bitmap_size.height * scale));
	margin = D2D1::SizeF(
		std::min(margin.width, 0.5f),
		std::min(margin.height, 0.5f));

	auto centre_min = D2D1::Point2F(margin.width, margin.height);
	auto centre_max = D2D1::Point2F(1.0f - margin.width, 1.0f - margin.height);
	return {
		clamp(centre.x, centre_min.x, centre_max.x),
		clamp(centre.y, centre_min.y, centre_max.y)};
}

void Pane::image_zoom_transform(
	const float scale,
	const D2D1_POINT_2F& zoom_point_ss,
	const D2D1_POINT_2F& dpi_scale
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
	auto offset_max_is = D2D1::Point2F(
		std::max(0.0f, bitmap_size.width - rect_size(content()).width / image_scale),
		std::max(0.0f, bitmap_size.height - rect_size(content()).height / image_scale));
	offset_is = D2D1::Point2F(
		clamp(offset_is.x, 0.0f, offset_max_is.x),
		clamp(offset_is.y, 0.0f, offset_max_is.y));

	// calculate actual centre
	// using pane size OR image size (whichever is smaller)
	auto image_extent = D2D1::SizeF(
		std::min(rect_size(content()).width, bitmap_size.width * image_scale),
		std::min(rect_size(content()).height, bitmap_size.height * image_scale));
	auto centre_ss = D2D1::Point2F(
		offset_is.x*image_scale + image_extent.width/2.0f,
		offset_is.y*image_scale + image_extent.height/2.0f);
	image_centre = D2D1::Point2F(
			centre_ss.x / image_scale / bitmap_size.width,
			centre_ss.y / image_scale / bitmap_size.height);

	// transform centre in old scale to centre in new scale
	image_centre = D2D1::Point2F(
		image_centre.x + zoom_point_ss.x / bitmap_size.width * (1.0f / image_scale - 1.0f / scale),
		image_centre.y + zoom_point_ss.y / bitmap_size.height * (1.0f / image_scale - 1.0f / scale));

	image_scale = scale;

	image_centre = clamp_centre(rect_size(content()), image->get_bitmap_size(dpi_scale), image_scale, image_centre);
}

void Pane::set_image_centre_from_other_pane(
	const Pane& pane_other,
	const D2D1_POINT_2F& dpi_scale
) {
	auto margin_other = D2D1::SizeF(
		rect_size(pane_other.content()).width / 2,
		rect_size(pane_other.content()).height / 2);
	auto margin_this = D2D1::SizeF(
		rect_size(content()).width / 2,
		rect_size(content()).height / 2);

	auto bitmap_size = image->get_bitmap_size(dpi_scale);
	auto bitmap_size_other = pane_other.image->get_bitmap_size(dpi_scale);

	auto centre_other_ss = D2D1::Point2F(
		pane_other.image_centre.x * bitmap_size_other.width * pane_other.image_scale,
		pane_other.image_centre.y * bitmap_size_other.height * pane_other.image_scale);

	auto panning_freedom_other = D2D1::SizeF(
		std::max(0.0f, bitmap_size_other.width * pane_other.image_scale - 2 * margin_other.width),
		std::max(0.0f, bitmap_size_other.height * pane_other.image_scale - 2 * margin_other.height));
	auto panning_freedom_this = D2D1::SizeF(
		std::max(0.0f, bitmap_size.width * image_scale - 2 * margin_this.width),
		std::max(0.0f, bitmap_size.height * image_scale - 2 * margin_this.height));

	// if other image cannot be panned, don't bother copying its
	// centre since it will always be placed at (0.5, 0.5)
	if (panning_freedom_other.width == 0 && panning_freedom_other.height == 0)
		return;

	auto panning_normalized = D2D1::Point2F(
		panning_freedom_other.width == 0 ? 0.5f : (centre_other_ss.x - margin_other.width) / panning_freedom_other.width,
		panning_freedom_other.height == 0 ? 0.5f : (centre_other_ss.y - margin_other.height) / panning_freedom_other.height);

	auto centre_this_ss = D2D1::Point2F(
		panning_normalized.x * panning_freedom_this.width + margin_this.width,
		panning_normalized.y * panning_freedom_this.height + margin_this.height);

	image_centre = D2D1::Point2F(
		centre_this_ss.x / bitmap_size.width / image_scale,
		centre_this_ss.y / bitmap_size.height / image_scale);

	image_centre = clamp_centre(rect_size(content()), image->get_bitmap_size(dpi_scale), image_scale, image_centre);
}

void Pane::translate_image_centre(const D2D1_POINT_2F& translation_isn, const D2D1_POINT_2F& dpi_scale) {
	image_centre += translation_isn;
	image_centre = clamp_centre(rect_size(content()), image->get_bitmap_size(dpi_scale), image_scale, image_centre);
}
