#include "shared.h"

#include "window.h"

#include "d2d.h"
#include "image.h"

#include "shared/numeric_cast.h"

#include <atomic>

#include <WindowsX.h>

Window::Window(const std::wstring& title, const D2D1_SIZE_U& size_min, const HICON icon) {
	et = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);

	D2D1_POINT_2F dpi;
	d2d_factory->GetDesktopDpi(&dpi.x, &dpi.y);
	scale = {dpi.x / 96, dpi.y / 96};
	debug_log << L"dpi: " << dpi << std::endl;
	debug_log << L"scale: " << scale << std::endl;
	assert(scale.x >= 1 && scale.y >= 1);

	this->title = title;
	this->size_min = {scale.x * to_dip_x(size_min.width), scale.y * to_dip_y(size_min.height)};

	lmb_down = false;
	layout_valid = false;

	INITCOMMONCONTROLSEX icc;
	icc.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
	icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	et = InitCommonControlsEx(&icc);

	WNDCLASSEX window_class = {
		sizeof(WNDCLASSEX), CS_CLASSDC, static_window_procedure, 0, 0,
		et = GetModuleHandle(nullptr), icon, nullptr, nullptr,
		nullptr, title.c_str(), nullptr
	};
	et = RegisterClassEx(&window_class);

	hwnd = et = CreateWindowEx(
		0, title.c_str(), title.c_str(),
		WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		GetDesktopWindow(), nullptr, window_class.hInstance, this
	);
	focus = hwnd;

	// make window at least as large as size_min
	RECT r;
	et = GetWindowRect(hwnd, &r);
	D2D1_SIZE_F s = {
		std::max(to_dip_x(r.right - r.left), this->size_min.width),
		std::max(to_dip_y(r.bottom - r.top), this->size_min.height)};
	et = SetWindowPos(
		hwnd, nullptr, 0, 0, to_dp_x(s.width), to_dp_y(s.height),
		SWP_NOMOVE | SWP_SHOWWINDOW | SWP_NOZORDER);

	#if 0
	HMENU menu = et = CreateMenu();
	std::vector<HMENU> menu_stack;
	menu_stack.push_back(menu);

	int id = 0;

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), L"File");
	menu_stack.push_back(menu);

	et = AppendMenu(menu_stack.back(), 0, id++, L"&New scan...\tCtrl+N");
	et = AppendMenu(menu_stack.back(), 0, id++, L"Exit");

	menu_stack.pop_back();

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), L"Scoring");
	menu_stack.push_back(menu);

	et = AppendMenu(menu_stack.back(), 0, id++, L"&Visual similarity\tCtrl+V");
	et = AppendMenu(menu_stack.back(), 0, id++, L"&Time difference (metadata)\tCtrl+T");
	et = AppendMenu(menu_stack.back(), 0, id++, L"&Positioning distance (metadata)\tCtrl+P");
	et = AppendMenu(menu_stack.back(), MF_CHECKED, id++, L"&Combined\tCtrl+C");

	menu_stack.pop_back();

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), L"Filters");
	menu_stack.push_back(menu);

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), L"Minimum score");
	menu_stack.push_back(menu);

	et = AppendMenu(menu_stack.back(), 0, id++, L"Exact match");
	et = AppendMenu(menu_stack.back(), 0, id++, L"Excellent match");
	et = AppendMenu(menu_stack.back(), MF_CHECKED|MFT_RADIOCHECK, id++, L"Good match");
	et = AppendMenu(menu_stack.back(), 0, id++, L"Fair match");
	et = AppendMenu(menu_stack.back(), 0, id++, L"Poor match");

	menu_stack.pop_back();

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), L"Folder restrictions");
	menu_stack.push_back(menu);

	et = AppendMenu(menu_stack.back(), MF_CHECKED, id++,  L"Images in a pair can be anywhere");
	et = AppendMenu(menu_stack.back(), 0, id++, L"Images in a pair must be in different folders");
	et = AppendMenu(menu_stack.back(), 0, id++, L"Images in a pair must be in the same folder");

	menu_stack.pop_back();

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), L"Maximum pair age");
	menu_stack.push_back(menu);

	et = AppendMenu(menu_stack.back(), MF_CHECKED, id++, L"Unlimited");
	et = AppendMenu(menu_stack.back(), 0, id++, L"One year");
	et = AppendMenu(menu_stack.back(), 0, id++, L"One month");
	et = AppendMenu(menu_stack.back(), 0, id++, L"One week");
	et = AppendMenu(menu_stack.back(), 0, id++, L"One day");

	menu_stack.pop_back();
	menu_stack.pop_back();

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), L"Help");
	menu_stack.push_back(menu);

	et = AppendMenu(menu_stack.back(), 0, id++, L"About Pixiple...");

	menu_stack.pop_back();

	assert(menu_stack.size() == 1);
	et = SetMenu(hwnd, menu_stack.back());

	et = CheckMenuRadioItem(menu_stack.back(), 2, 5, 5, MF_BYCOMMAND);
	et = CheckMenuItem(menu, 4, MF_BYCOMMAND | MF_CHECKED);
	#endif
}

Window::~Window() {
	reset();

	et = DestroyWindow(hwnd);
	hwnd = nullptr;

	et = UnregisterClass(title.c_str(), et = GetModuleHandle(nullptr));
}

HWND Window::get_handle() const {
	assert(hwnd);
	return hwnd;
}

void Window::reset() {
	while (has_event())
		get_event();
	panes.clear();
	edges.clear();

	assert(menu_stack.size() == 0 || menu_stack.size() == 1);
	if (!menu_stack.empty()) {
		et = SetMenu(hwnd, nullptr);
		et = DestroyMenu(menu_stack.front());
		menu_stack.clear();
	}
}

void Window::set_dirty() {
	et = InvalidateRect(hwnd, nullptr, false);
}

void Window::message_box(const std::wstring& text) const {
	et = MessageBox(hwnd, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

bool Window::has_event() const {
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			queue_event(Event(Event::Type::quit));
			ShowWindow(hwnd, SW_HIDE);
		} else if(!IsDialogMessage(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return !events.empty();
}

Event Window::get_event() const {
	while (!has_event()) {
		MSG msg;
		et = GetMessage(&msg, nullptr, 0, 0) != -1;
		if (msg.message == WM_QUIT) {
			queue_event(Event(Event::Type::quit));
			ShowWindow(hwnd, SW_HIDE);
		} else if(!IsDialogMessage(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	Event event = events.front();
	events.pop_front();
	return event;
}

bool Window::quit_event_seen() const {
	LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
	return !(style & WS_VISIBLE);
}

D2D1_SIZE_F Window::get_size() const {
	return size;
}

D2D1_POINT_2F Window::get_scale() const {
	return scale;
}

int Window::to_dp_x(const float& dip_x) const {
	return static_cast<int>(dip_x * scale.x);
}

int Window::to_dp_y(const float& dip_y) const {
	return static_cast<int>(dip_y * scale.y);
}

float Window::to_dip_x(const int& dp_x) const {
	return static_cast<float>(dp_x / scale.x);
}

float Window::to_dip_y(const int& dp_y) const {
	return static_cast<float>(dp_y / scale.y);
}

D2D1_POINT_2F Window::get_mouse_position() const {
	return mouse_position;
}

void Window::set_cursor(const int pane, LPCTSTR cursor_name) {
	panes[pane].set_cursor(cursor_name);

	int p = get_pane(get_mouse_position());
	if (p == pane)
		SetCursor(panes[pane].get_cursor());
}

void Window::push_menu_level(const std::wstring& label) {
	HMENU menu;

	if (menu_stack.empty()) {
		menu = et = CreateMenu();
		menu_stack.push_back(menu);
	}

	menu = et = CreateMenu();
	et = AppendMenu(menu_stack.back(), MF_POPUP, reinterpret_cast<UINT_PTR>(menu), label.c_str());
	menu_stack.push_back(menu);
}

void Window::pop_menu_level() {
	assert(!menu_stack.empty());
	menu_stack.pop_back();

	if (menu_stack.size() == 1)
		et = SetMenu(hwnd, menu_stack.front());
}

void Window::add_menu_item(const std::wstring& label, const int button_id, const int checkmark_group) {
	assert(!menu_stack.empty());
	et = AppendMenu(menu_stack.back(), 0, button_id, label.c_str());
	if (checkmark_group != -1)
		menu_groups.push_back(std::make_pair(button_id, checkmark_group));
}

void Window::set_menu_item_checked(const int button_id) {
	assert(!menu_stack.empty());

	int group_id = 0;
	bool found = false;
	for (const auto& p : menu_groups) {
		if (p.first == button_id) {
			group_id = p.second;
			found = true;
			break;
		}
	}
	assert(found);

	int button_id_first = -1;
	int button_id_last = -1;

	for (const auto& p : menu_groups) {
		if (p.second == group_id) {
			if (button_id_first == -1)
				button_id_first = p.first;
			button_id_last = p.first;
		}
	}
	assert(button_id_first != -1);
	assert(button_id_last != -1);
	assert(button_id_first < button_id_last);

	et = CheckMenuRadioItem(menu_stack.front(), button_id_first, button_id_last, button_id, MF_BYCOMMAND | MF_CHECKED) != -1;
}

void Window::add_edge(float relative_position) {
	edges.push_back(Edge(relative_position));
}

void Window::add_pane(
	const int index,
	const int left, const int top, const int right, const int bottom,
	const D2D1_RECT_F margin,
	bool fixed_width, bool fixed_height,
	D2D1_COLOR_F colour) {

	assert(left < int(edges.size()));
	assert(top < int(edges.size()));
	assert(right < int(edges.size()));
	assert(bottom < int(edges.size()));

	assert(std::size_t(index) == panes.size());

	panes.push_back({
		this,
		&edges[left], &edges[top], &edges[right], &edges[bottom],
		margin, fixed_width, fixed_height, colour});

	layout_valid = false;
	et = InvalidateRect(hwnd, nullptr, false);
}

int Window::get_pane(const D2D1_POINT_2F& mouse_position) const {
	const_cast<Window*>(this)->update_layout();

	int i = 0;
	for (auto& pane : panes) {
		if (pane.is_inside(mouse_position))
			return i;
		i++;
	}
	return -1;
}

void Window::click_button(const int button_index) {
	et = PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(button_index, BN_CLICKED), 0);
}

D2D1_RECT_F Window::content(const int pane_index) const {
	const_cast<Window*>(this)->update_layout();

	return panes[pane_index].content();
}

void Window::set_text(const int pane_index, const std::wstring& text, const std::vector<std::pair<std::size_t, std::size_t>>& bold_ranges) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].set_text(text, bold_ranges);
}

void Window::set_progressbar_progress(const int pane_index, const float progress) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].set_progressbar_progress(progress);
}

void Window::add_button(const int pane_index, const int button_id, const std::wstring& label) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].add_button(button_id, label);
}

void Window::add_combobox(const int pane_index, const int button_id, const std::vector<std::wstring>& items) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].add_combobox(button_id, items);
}

#if 0
static bool is_button_window(const HWND window) {
	wchar_t class_name[16];
	assert(sizeof(class_name) / sizeof(wchar_t) >= std::wstring(WC_BUTTON).length() + 1);
	auto r = GetClassName(window, class_name, sizeof(class_name) / sizeof(wchar_t));
	if (r == 0)
		return false;
	class_name[sizeof(class_name) / sizeof(wchar_t) - 1] = L'\0';
	return std::wstring(WC_BUTTON) == std::wstring(class_name);
}
#endif

void Window::set_button_state(const int button_id, const bool enable) {
#if 0
	HWND w = et = GetWindow(hwnd, GW_CHILD);
	HWND bw = nullptr;
	while (w != nullptr && bw == nullptr) {
		int bid = reinterpret_cast<int>(GetMenu(w));
		if (bid == button_id)
			bw = w;
		w = GetNextWindow(w, GW_HWNDNEXT);
	}
	et = bw;
#endif
	auto bw = et = GetDlgItem(hwnd, button_id);

	if (enable) {
		EnableWindow(bw, true);
		if ((GetWindowLongPtr(bw, GWL_STYLE) & BS_TYPEMASK) == BS_DEFPUSHBUTTON) {
			focus = bw;
			SetFocus(focus);
		}
	} else {
		if (bw == GetFocus()) {
			focus = hwnd;
			SetFocus(focus);
		}
		EnableWindow(bw, false);
	}
}

void Window::set_button_focus(const int button_id) {
#if 0
	HWND w = et = GetWindow(hwnd, GW_CHILD);
	while (w) {
		LONG_PTR style = GetWindowLongPtr(w, GWL_STYLE);
		bool is_button =
			(style & BS_TYPEMASK) == BS_PUSHBUTTON ||
			(style & BS_TYPEMASK) == BS_DEFPUSHBUTTON;
		if (is_button) {
			style &= ~BS_TYPEMASK;
			if (reinterpret_cast<int>(GetMenu(w)) == button_id) {
				style |= BS_DEFPUSHBUTTON;
				SetFocus(w);
			} else {
				style |= BS_PUSHBUTTON;
			}
			et = SetWindowLongPtr(w, GWL_STYLE, style);
		}
		w = GetNextWindow(w, GW_HWNDNEXT);
	}
#endif
	auto bw = et = GetDlgItem(hwnd, button_id);

	HWND w = et = GetWindow(hwnd, GW_CHILD);
	while (w) {
		LONG_PTR style = GetWindowLongPtr(w, GWL_STYLE);
		bool is_button =
			(style & BS_TYPEMASK) == BS_PUSHBUTTON ||
			(style & BS_TYPEMASK) == BS_DEFPUSHBUTTON;
		if (is_button) {
			style &= ~BS_TYPEMASK;
			if (w == bw) {
				style |= BS_DEFPUSHBUTTON;
				focus = w;
				SetFocus(focus);
			} else {
				style |= BS_PUSHBUTTON;
			}
			et = SetWindowLongPtr(w, GWL_STYLE, style);
		}
		w = GetNextWindow(w, GW_HWNDNEXT);
	}
}

std::shared_ptr<Image> Window::get_image(const int pane_index) const {
	assert(pane_index < int(edges.size()));
	return panes[pane_index].get_image();
}

void Window::set_image(const int pane_index, std::shared_ptr<Image> image) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].set_image(image);
}

float Window::get_image_scale(const int pane_index) {
	assert(pane_index < int(edges.size()));
	return panes[pane_index].get_image_scale();
}

void Window::set_image_scale(const int pane_index, const float scale) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].set_image_scale(scale);
}

void Window::image_zoom_transform(
	const int pane_index,
	const float scale,
	const D2D1_POINT_2F& zoom_point_ss) {

	assert(pane_index < int(edges.size()));
	panes[pane_index].image_zoom_transform(scale, zoom_point_ss, get_scale());
}

void Window::set_image_centre_from_other_pane(const int pane_index, const int pane_index_other) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].set_image_centre_from_other_pane(panes[pane_index_other], get_scale());
}

void Window::translate_image_centre(const int pane_index, const D2D1_POINT_2F& translation_isn) {
	assert(pane_index < int(edges.size()));
	panes[pane_index].translate_image_centre(translation_isn, get_scale());
}

LRESULT WINAPI Window::static_window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	Window* window;

	if (msg == WM_NCCREATE) {
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
		window = static_cast<Window*>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	} else {
		window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	return window->window_procedure(hwnd, msg, wparam, lparam);
}

LRESULT WINAPI Window::window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DISPLAYCHANGE)
		debug_log << L"WM_DISPLAYCHANGE" << std::endl;

	D2D1_POINT_2F mouse_position_delta;
	if (this) {
		POINT mp;
		et = GetCursorPos(&mp);
		et = ScreenToClient(hwnd, &mp);
		auto mouse_position_new = D2D1::Point2F(to_dip_x(mp.x), to_dip_y(mp.y));
		mouse_position_delta = {mouse_position.x - mouse_position_new.x, mouse_position.y - mouse_position_new.y};
		mouse_position = mouse_position_new;
	}

	if (msg == DM_GETDEFID) {
		// WM_USER+0, used by IsDialogMessage()
	} else if (msg == DM_SETDEFID) {
		// WM_USER+1, used by IsDialogMessage()
	} else if (msg == WM_ACTIVATE) {
		if (wparam == WA_INACTIVE) {
			auto f = GetFocus();
			if (f && IsChild(hwnd, f))
				focus = f;
		} else {
			SetFocus(focus);
		}
	} else if (msg == WM_CLOSE) {
		queue_event(Event(Event::Type::quit));
		ShowWindow(hwnd, SW_HIDE);
	} else if (msg == WM_COMMAND) {
		if (HIWORD(wparam) == BN_CLICKED) {
			Event e(Event::Type::button);
			e.button_id = numeric_cast<int>(wparam);
			queue_event(e);
		}
	} else if (msg == WM_KEYDOWN) {
		Event e(Event::Type::key);
		e.key_code = static_cast<std::uint8_t>(wparam);
		queue_event(e);
	} else if (msg == WM_LBUTTONDOWN) {
		SetCapture(hwnd);
	} else if (msg == WM_LBUTTONUP) {
		et = ReleaseCapture();
	} else if (msg == WM_MOUSEMOVE) {
		if (wparam & MK_LBUTTON && !lmb_down)
			lmb_down_mouse_position = mouse_position;
		lmb_down = wparam & MK_LBUTTON;

		bool drag_event =
			lmb_down &&
			(mouse_position_delta.x != 0.0f || mouse_position_delta.y != 0.0f);
		if (drag_event) {
			Event e(Event::Type::drag);
			e.drag_mouse_position_delta = mouse_position_delta;
			e.drag_mouse_position_start = lmb_down_mouse_position;
			queue_event(e);
		}
	} else if (msg == WM_MOUSEWHEEL) {
		Event e(Event::Type::wheel);
		e.wheel_count_delta = GET_WHEEL_DELTA_WPARAM(wparam);
		queue_event(e);
	} else if (msg == WM_PAINT) {
		paint();
	} else if (msg == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT) {
		int pane = get_pane(get_mouse_position());
		if (pane >= 0)
			SetCursor(panes[pane].get_cursor());
		return true;
	} else if (msg == WM_SIZE) {
		D2D1_SIZE_U s = D2D1::SizeU(LOWORD(lparam), HIWORD(lparam));
		if (s.width != 0 && s.height != 0) {
			if (render_target) {
				et = render_target->Resize(s);
				size = render_target->GetSize();
			}

			queue_event(Event(Event::Type::size));
			et = InvalidateRect(hwnd, nullptr, false);
			layout_valid = false;
		}
	} else if (msg == WM_TIMER) {
		if (wparam == progressbar_timer_id)
			queue_event(Event(Event::Type::none));
		else
			assert(false);
	} else if (msg == WM_WINDOWPOSCHANGING) {
		WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lparam);
		wp->cx = std::max(to_dp_x(size_min.width), wp->cx + wp->cx%2);
		wp->cy = std::max(to_dp_y(size_min.height), wp->cy);
	} else {
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return 0;
}

void Window::queue_event(const Event& event) const {
	// consolidate events of the same type
	if (event.type == Event::Type::drag) {
		auto i = std::find_if(events.begin(), events.end(),
			[&](const Event& e) { return e.type == event.type; });
		if (i != events.end()) {
			i->drag_mouse_position_delta = D2D1::Point2F(
				i->drag_mouse_position_delta.x + event.drag_mouse_position_delta.x,
				i->drag_mouse_position_delta.y + event.drag_mouse_position_delta.y);
			return;
		}
	} else if(event.type == Event::Type::button) {
		auto i = std::find_if(events.begin(), events.end(),
			[&](const Event& e) { return e.type == event.type && e.key_code == event.key_code; });
		if (i != events.end())
			return;
	} else if(event.type == Event::Type::none || event.type == Event::Type::quit || event.type == Event::Type::size) {
		auto i = std::find_if(events.begin(), events.end(),
			[&](const Event& e) { return e.type == event.type; });
		if (i != events.end())
			return;
	}

	events.push_back(event);

	assert(events.size() < 20);
}

void Window::update_layout() {
	if (layout_valid)
		return;

	for (auto& edge : edges)
		edge.reset_position();

	D2D1_SIZE_F size = get_size();

	for (auto& pane : panes) {
		if (pane.has_width()) {
			if (pane.edge_left->has_position()) {
				float p = pane.edge_left->get_position(size.width) + pane.get_width();
				if (pane.edge_right->has_position())
					pane.edge_right->set_position(std::max(p, pane.edge_right->get_position(size.width)));
				else
					pane.edge_right->set_position(p);
			} else if (pane.edge_right->has_position()) {
				float p = pane.edge_right->get_position(size.width) - pane.get_width();
				if (pane.edge_left->has_position())
					pane.edge_left->set_position(std::min(p, pane.edge_left->get_position(size.width)));
				else
					pane.edge_left->set_position(p);
			}
		}
		if (pane.has_height()) {
			if (pane.edge_top->has_position()) {
				float p = pane.edge_top->get_position(size.height) + pane.get_height();
				if (pane.edge_bottom->has_position())
					pane.edge_bottom->set_position(std::max(p, pane.edge_bottom->get_position(size.height)));
				else
					pane.edge_bottom->set_position(p);
			} else if (pane.edge_bottom->has_position()) {
				float p = pane.edge_bottom->get_position(size.height) - pane.get_height();
				if (pane.edge_top->has_position())
					pane.edge_top->set_position(std::min(p, pane.edge_top->get_position(size.height)));
				else
					pane.edge_top->set_position(p);
			}
		}
	}

	for (auto& pane : panes)
		pane.update();

	et = InvalidateRect(hwnd, nullptr, false);
	layout_valid = true;
}

void Window::paint() const {
	const_cast<Window*>(this)->update_layout();

	assert(menu_stack.size() <= 1);
	if (render_target == nullptr) {
		RECT r;
		et = GetClientRect(hwnd, &r);
		et = d2d_factory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(r.right - r.left, r.bottom - r.top)),
			&render_target);
		render_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		size = render_target->GetSize();
		Image::clear_cache();
	}

	// when paint() is called during ctrl-alt-del screen, nothing is drawn
	// (because of different render target?). when window becomes visible, it is
	// empty and no WM_PAINT is sent so it remains empty. therefore, the
	// CheckWindowState/Sleep hack below seems to be required.
	
	// render target occluded? if so, skip paint and do not validate dirty
	// region so that another WM_PAINT will be sent.
	if (render_target->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED) {
		debug_log << L"D2D1_WINDOW_STATE_OCCLUDED" << std::endl;
		Sleep(200);
		return;
	}

	PAINTSTRUCT ps;
	et = BeginPaint(hwnd, &ps);
	render_target->BeginDraw();

	for (auto& pane : panes)
		pane.draw(render_target);

	HRESULT hr;
	hr = render_target->EndDraw();
	EndPaint(hwnd, &ps);

	// did we draw on occluded render target? if so, make sure another WM_PAINT
	// is sent. apparently we do not know whether we're occluded until after
	// EndDraw is called, so the check before BeginPaint is not sufficient on
	// its own.
	if (render_target->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED) {
		debug_log << L"D2D1_WINDOW_STATE_OCCLUDED" << std::endl;
		et = InvalidateRect(hwnd, nullptr, false);
	}

	if (hr == D2DERR_RECREATE_TARGET) {
		debug_log << L"D2DERR_RECREATE_TARGET" << std::endl;
		render_target = nullptr;
		et = InvalidateRect(hwnd, nullptr, false);
	} else {
		et = hr;
	}
}

