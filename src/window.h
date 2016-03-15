#pragma once

#include "drop_target.h"
#include "edge.h"
#include "pane.h"

#include "shared/com.h"
#include "shared/vector.h"

#include <future>
#include <list>
#include <thread>

#include <d2d1.h>

struct Event {
	enum class Type {button, drag, items, key, none, quit, size, wheel} type;

	// button
	int button_id;

	// drag
	Vector2f drag_mouse_position_delta;
	Point2f drag_mouse_position_start;

	// items
	std::vector<ComPtr<IShellItem>> items;

	//  key
	uint8_t key_code;

	// wheel
	int wheel_count_delta;

	Event(Type type) : type{type} {
	}
};

class Window {
public:
	bool layout_valid;

	Window::Window(const std::wstring& title, const D2D1_SIZE_U& size_min, const HICON icon);
	~Window();

	HWND get_handle() const;

	void reset();
	void set_dirty() const;

	void message_box(const std::wstring& text) const;

	bool has_event() const;
	Event get_event() const;
	bool quit_event_seen() const;

	Size2f get_size() const;

	Vector2f get_scale() const;
	int to_dp_x(const float& dip_x) const;
	int to_dp_y(const float& dip_y) const;
	float to_dip_x(const int& dp_x) const;
	float to_dip_y(const int& dp_y) const;

	Point2f get_mouse_position() const;

	void set_cursor(const int pane, LPCTSTR cursor_name);

	void set_drop_target(bool enable);

	void push_menu_level(const std::wstring& label);
	void pop_menu_level();
	void add_menu_item(const std::wstring& label, const int button_id, const int checkmark_group = -1);
	void set_menu_item_checked(const int button_id);

	void add_edge(float relative_position = -1);
	void add_pane(
		const int left, const int top, const int right, const int bottom,
		const D2D1_RECT_F margin,
		const bool fixed_width, const bool fixed_height,
		const Colour colour);

	D2D1_RECT_F container(const int pane_index) const;
	D2D1_RECT_F content(const int pane_index) const;

	int get_pane(const Point2f& mouse_position) const;

	void click_button(const int button_index);

	void set_text(const int pane_index, const std::wstring& text, const std::vector<std::pair<std::size_t, std::size_t>>& bold_ranges = std::vector<std::pair<std::size_t, std::size_t>>(), bool centred = false);
	void set_progressbar_progress(const int pane_index, const float progress);

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
		const Point2f& zoom_point_ss);
	void set_image_centre_from_other_pane(
		const int pane_index,
		const int pane_index_other);
	void translate_image_centre(
		const int pane_index,
		const Vector2f& translation_isn);

private:
	static const int progressbar_timer_id = 1;
	static const int progressbar_timer_ms = 200;

	HWND hwnd;

	mutable std::list<Event> events;

	mutable Size2f size{0, 0};
	Size2f size_min;
	Vector2f scale;
	Point2f mouse_position;
	HCURSOR cursor;
	HWND focus;

	std::wstring title;

	bool lmb_down;
	Point2f lmb_down_mouse_position;

	std::vector<HMENU> menu_stack;
	std::vector<std::pair<int, int>> menu_groups;

	ComPtr<ID2D1Factory> d2d_factory;
	mutable ComPtr<ID2D1HwndRenderTarget> render_target;

	std::vector<Pane> panes;
	std::vector<Edge> edges;

	ComPtr<IDropTarget> drop_target;

	static LRESULT WINAPI static_window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	LRESULT WINAPI window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void queue_event(const Event& event) const;

	void update_layout();
	void paint() const;

	friend class DropTarget;
};
