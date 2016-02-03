#pragma once

#include "edge.h"
#include "pane.h"

#include "shared/com.h"

#include <future>
#include <list>
#include <thread>

#include <d2d1.h>

struct Event {
	enum class Type { button, drag, key, none, quit, size, wheel } type;

	// button
	int button_id;

	// drag
	D2D1_POINT_2F drag_mouse_position_delta;
	D2D1_POINT_2F drag_mouse_position_start;

	//  key
	uint8_t key_code;

	// wheel
	int wheel_count_delta;

	Event(Type type) : type{type} { }
};

class Window {
public:
	bool layout_valid;

	Window::Window(const std::wstring& title, const D2D1_SIZE_U& size_min, const HICON icon);
	~Window();

	HWND get_handle() const;

	void reset();
	void set_dirty();

	void message_box(const std::wstring& text) const;

	bool has_event() const;
	Event get_event() const;
	bool quit_event_seen() const;

	D2D1_SIZE_F get_size() const;

	D2D1_POINT_2F get_scale() const;
	int to_dp_x(const float& dip_x) const;
	int to_dp_y(const float& dip_y) const;
	float to_dip_x(const int& dp_x) const;
	float to_dip_y(const int& dp_y) const;

	D2D1_POINT_2F get_mouse_position() const;

	void set_cursor(const int pane, LPCTSTR cursor_name);

	void push_menu_level(const std::wstring& label);
	void pop_menu_level();
	void add_menu_item(const std::wstring& label, const int button_id, const int checkmark_group = -1);
	void set_menu_item_checked(const int button_id);

	void add_edge(float relative_position = -1);
	void add_pane(
		const int index,
		const int left, const int top, const int right, const int bottom,
		const D2D1_RECT_F margin,
		bool fixed_width, bool fixed_height,
		D2D1_COLOR_F colour);

	D2D1_RECT_F container(const int pane_index) const;
	D2D1_RECT_F content(const int pane_index) const;

	int get_pane(const D2D1_POINT_2F& mouse_position) const;

	void click_button(const int button_index);

	void set_text(const int pane_index, const std::wstring& text, const std::vector<std::pair<std::size_t, std::size_t>>& bold_ranges = std::vector<std::pair<std::size_t, std::size_t>>());
	void set_progressbar_progress(const int pane_index, const std::size_t value, const std::size_t max_value);

	void add_button(const int pane_index, const int button_id, const std::wstring& label);
	void add_combobox(const int pane_index, const int button_id, const std::vector<std::wstring>& items);
	void set_button_state(const int button_id, const bool enable);
	void set_button_focus(const int button_id);

	std::shared_ptr<Image> get_image(const int pane_index) const;
	void set_image(const int pane_index, std::shared_ptr<Image> image);
	float get_image_scale(const int pane_index);
	void set_image_scale(const int pane_index, const float scale);
	void image_zoom_transform(
		const int pane_index,
		const float scale,
		const D2D1_POINT_2F& zoom_point_ss);
	void set_image_centre_from_other_pane(
		const int pane_index,
		const int pane_index_other);
	void translate_image_centre(
		const int pane_index,
		const D2D1_POINT_2F& translation_isn);

private:
	static const int progressbar_timer_id = 1;
	static const int progressbar_timer_ms = 200;

	HWND hwnd;

	mutable std::list<Event> events;

	mutable D2D1_SIZE_F size;
	D2D1_SIZE_F size_min;
	D2D1_POINT_2F scale;
	D2D1_POINT_2F mouse_position;
	HCURSOR cursor;

	std::wstring title;

	bool lmb_down;
	D2D1_POINT_2F lmb_down_mouse_position;

	std::vector<HMENU> menu_stack;
	std::vector<std::pair<int, int>> menu_groups;

	ComPtr<ID2D1Factory> d2d_factory;
	mutable ComPtr<ID2D1HwndRenderTarget> render_target;

	std::vector<Pane> panes;
	std::vector<Edge> edges;

	static LRESULT WINAPI static_window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	LRESULT WINAPI window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void queue_event(const Event& event) const;

	void update_layout();
	void paint() const;
};
