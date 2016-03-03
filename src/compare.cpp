#include "shared.h"

#include "d2d.h"
#include "image.h"
#include "image_pair.h"
#include "window.h"

#include "shared/map.h"

#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>

#include <shellapi.h>
#include <windowsx.h>

enum {
	pane_pair_info, pane_pair_buttons,
	pane_info_header, pane_info_left, pane_info_right,
	pane_scale_header, pane_scale_left, pane_buttons_left, pane_scale_right, pane_buttons_right,
	pane_image_left, pane_image_right
};

const auto scale_level_exponent_min = -6;
const auto scale_level_exponent_max = 6;

float get_fit_scale(const Size2f pane_size, const Size2f bitmap_size) {
	auto fit_scale = std::min(
		pane_size.w / bitmap_size.w,
		pane_size.h / bitmap_size.h);
	return clamp(
		fit_scale,
		pow(2.0f, scale_level_exponent_min),
		pow(2.0f, scale_level_exponent_max));
}

std::vector<std::pair<float, float>> get_scale_levels(
	const float fit_scale_left,
	const float fit_scale_right,
	const float swapped_left_right_scale_ratio
) {
	std::vector<std::pair<float, float>> scale_level_pairs;

	// add fit scales (with corresponding scale for other image)
	scale_level_pairs.push_back({fit_scale_left, fit_scale_left * swapped_left_right_scale_ratio});
	scale_level_pairs.push_back({fit_scale_right / swapped_left_right_scale_ratio, fit_scale_right});

	// find smallast scale to include
	auto min_scale = std::min({
		scale_level_pairs[0].first,
		scale_level_pairs[0].second,
		scale_level_pairs[1].first,
		scale_level_pairs[1].second});
	min_scale = std::min(min_scale, 1.0f);

	// add fixed scales (with corresponding scale for other image)
	for (auto i = scale_level_exponent_min; i <= scale_level_exponent_max; i++) {
		auto sl = pow(2.0f, i);
		auto sl1 = sl * swapped_left_right_scale_ratio;
		auto sl2 = sl / swapped_left_right_scale_ratio;

		if (sl >= min_scale && sl1 >= min_scale)
			scale_level_pairs.push_back({sl, sl1});
		if (sl >= min_scale && sl2 >= min_scale)
			scale_level_pairs.push_back({sl2, sl});
	}

	// remove pairs
	std::sort(scale_level_pairs.begin(), scale_level_pairs.end());
	scale_level_pairs.erase(std::unique(scale_level_pairs.begin(), scale_level_pairs.end()), scale_level_pairs.end());
	assert(!scale_level_pairs.empty());

	#ifdef _DEBUG
	// assert scales strictly ascending
	auto previous_first = 0.0f;
	auto previous_second = 0.0f;
	for (const auto& slp : scale_level_pairs) {
		assert(slp.first > previous_first);
		assert(slp.second > previous_second);
		previous_first = slp.first;
		previous_second = slp.second;
	}
	#endif

	return scale_level_pairs;
}

void zoom(
	Window& window,
	const std::vector<std::pair<float, float>> scale_levels,
	const int wheel_count_delta
) {
	// find pane to zoom in

	auto pane = window.get_pane(window.get_mouse_position());
	Point2f zoom_point;
	if (pane > 0 && window.get_image(pane)) {
		zoom_point = {
			window.get_mouse_position().x - (window.content(pane).left + window.content(pane).right) / 2.0f,
			window.get_mouse_position().y - (window.content(pane).top + window.content(pane).bottom) / 2.0f};
	} else {
		pane = pane_image_left;
		zoom_point = {0, 0};
	}

	// get the new scale

	float scale;
	float scale_other;

	if (wheel_count_delta < 0) {
		// zoom out
		auto sl_i = std::find_if(scale_levels.crbegin(), scale_levels.crend(), [&](const std::pair<float, float>& scales) {
			return scales.first < window.get_image_scale(pane_image_left);
		});
		if (sl_i == scale_levels.crend())
			return;
		scale = sl_i->first;
		scale_other = sl_i->second;;
	} else {
		// zoom in
		auto sl_i = std::find_if(scale_levels.cbegin(), scale_levels.cend(), [&](const std::pair<float, float>& scales) {
			return scales.first > window.get_image_scale(pane_image_left);
		});
		if (sl_i == scale_levels.cend())
			return;
		scale = sl_i->first;
		scale_other = sl_i->second;;
	}

	if (pane == pane_image_right)
		std::swap(scale, scale_other);

	window.image_zoom_transform(pane, scale, zoom_point);

	auto pane_other = pane == pane_image_left ? pane_image_right : pane_image_left;
	window.set_image_scale(pane_other, scale_other);
	window.set_image_centre_from_other_pane(pane_other, pane);
}

std::size_t get_matching_text_length(const std::wstring& text1, const std::wstring& text2) {
	auto search_length = std::min(text1.length(), text2.length());
	std::size_t i = 0;
	while (i < search_length && text1[i] == text2[i])
		i++;
	return i;
}

std::wstring to_string(const std::chrono::system_clock::time_point& tp) {
	std::wostringstream ss;
	ss.imbue(std::locale(""));

	auto t = std::chrono::system_clock::to_time_t(tp);
	tm tm;
	er = localtime_s(&tm, &t) == 0;
	ss << std::put_time(&tm, L"%c");

	return ss.str();
}

void update_text_image_info(
	Window& window,
	const std::shared_ptr<const Image>& image,
	const std::shared_ptr<const Image>& image_other,
	const int pane,
	const float scale,
	const float fit_scale,
	const int scale_pane
) {
	std::wostringstream ss;
	ss.imbue(std::locale(""));

	std::vector<std::pair<std::size_t, std::size_t>> bold_ranges;

	std::size_t index = 0;

	// filename

	ss << image->get_path() << L"\n";
	auto matching_length = get_matching_text_length(image->get_path(), image_other->get_path());
	bold_ranges.push_back({index, matching_length});

	matching_length = get_matching_text_length(
		image->get_path().filename(), image_other->get_path().filename());
	bold_ranges.push_back(
		{index + image->get_path().parent_path().wstring().length() + 1, matching_length});
	index = ss.str().length();

	// file

	ss << image->get_file_size() << L" bytes, ";
	if (image->get_file_size() == image_other->get_file_size())
		bold_ranges.push_back({index, ss.str().length() - index});
	index = ss.str().length();

	ss << to_string(image->get_file_time()) << L", ";
	if (image->get_file_time() == image_other->get_file_time())
		bold_ranges.push_back({index, ss.str().length() - index});
	index = ss.str().length();

	ss  << L"hash " << image->get_file_hash() << L"\n";
	if (image->get_file_hash() == image_other->get_file_hash())
		bold_ranges.push_back({index, ss.str().length() - index});
	index = ss.str().length();

	// pixels

	ss << image->get_image_size().w << L" \u00d7 " << image->get_image_size().h << L", ";
	if (image->get_image_size().w == image_other->get_image_size().w && image->get_image_size().h == image_other->get_image_size().h)
		bold_ranges.push_back({index, ss.str().length() - index});
	index = ss.str().length();

	ss  << L"hash " << image->get_pixel_hash() << L"\n";
	if (image->get_pixel_hash() == image_other->get_pixel_hash())
		bold_ranges.push_back({index, ss.str().length() - index});
	index = ss.str().length();

	// metadata times

	auto image_other_metadata_times = image_other->get_metadata_times();
	for (auto t : image->get_metadata_times()) {
		ss << to_string(t);

		auto r = std::find(image_other_metadata_times.begin(), image_other_metadata_times.end(), t);
		if (r != image_other_metadata_times.end())
			bold_ranges.push_back({index, ss.str().length() - index});

		ss << L", ";
		index = ss.str().length();
	}

	// metadata camera

	if (!image->get_metadata_make_model().empty()) {
		std::wostringstream ssc;
		ssc << image->get_metadata_make_model();
		if (!image->get_metadata_camera_id().empty())
			ssc << L" " << image->get_metadata_camera_id();

		ss << ssc.str();

		std::wostringstream ssco;
		ssco << image_other->get_metadata_make_model();
		if (!image_other->get_metadata_camera_id().empty())
			ssco << L" " << image_other->get_metadata_camera_id();

		if (ssc.str() == ssco.str())
			bold_ranges.push_back({index, ss.str().length() - index});
		ss << L", ";
		index = ss.str().length();
	}

	// metadata position

	if (image->get_metadata_position().x != 0 && image->get_metadata_position().y != 0) {
		ss << L"(" << image->get_metadata_position().y << L", " << image->get_metadata_position().x << L")";
		if (image->get_metadata_position().x == image_other->get_metadata_position().x && image->get_metadata_position().y == image_other->get_metadata_position().y)
			bold_ranges.push_back({index, ss.str().length() - index});
		ss << L", ";
		index = ss.str().length();
	}

	// metadata image id

	if (!image->get_metadata_image_id().empty()) {
		ss << image->get_metadata_image_id();
		if (image->get_metadata_image_id() == image_other->get_metadata_image_id())
			bold_ranges.push_back({index, ss.str().length() - index});
		ss << L", ";
		index = ss.str().length();
	}

	// set text

	std::wstring s(ss.str());

	// remove trailing comma
	if (s.size() >= 2 && s[s.size()-2] == L',' && s[s.size()-1] == L' ')
		s.erase(s.size()-2, 2);

	window.set_text(pane, s, bold_ranges);
	ss.str(L"");

	// scale

	if (scale * 100.0f == floor(scale * 100.0f)) {
		ss << static_cast<int>(scale * 100.0f) << L" % of actual size";
	} else {
		ss << std::fixed << std::setprecision(1);
		ss << scale * 100.0f << L" % of actual size";
	}
	if (scale == fit_scale)
		ss << L" (fit pane)";
	window.set_text(scale_pane, ss.str());
}

void update_text(
	Window& window,
	const std::vector<ImagePair>& pairs,
	const std::vector<ImagePair>::const_iterator& pairs_it
) {
	std::wostringstream ss;
	ss.imbue(std::locale(""));
	ss << std::fixed;

	if (!pairs.empty()) {
		ss << L"Image pair " << 1 + distance(pairs.begin(), pairs_it) << L" of " << pairs.size() << L": ";
		ss << pairs_it->description();
	} else {
		ss << L"No images";
	}
	window.set_text(pane_pair_info, ss.str());
	ss.str(L"");

	ss << L"Path\n";
	ss << L"File\n";
	ss << L"Pixels\n";
	ss << L"Metadata";
	window.set_text(pane_info_header, ss.str());
	ss.str(L"");

	ss << L"Scale";
	window.set_text(pane_scale_header, ss.str());
	ss.str(L"");

	if (window.get_image(pane_image_left)) {
		update_text_image_info(
			window,
			window.get_image(pane_image_left),
			window.get_image(pane_image_right),
			pane_info_left,
			window.get_image_scale(pane_image_left),
			get_fit_scale(
				rect_size(window.content(pane_image_left)),
				window.get_image(pane_image_left)->get_bitmap_size(window.get_scale())),
			pane_scale_left);
	} else {
		window.set_text(pane_info_left, L"");
		window.set_text(pane_scale_left, L"");
	}

	if (window.get_image(pane_image_right)) {
		update_text_image_info(
			window,
			window.get_image(pane_image_right),
			window.get_image(pane_image_left),
			pane_info_right,
			window.get_image_scale(pane_image_right),
			get_fit_scale(
				rect_size(window.content(pane_image_right)),
				window.get_image(pane_image_right)->get_bitmap_size(window.get_scale())),
			pane_scale_right);
	} else {
		window.set_text(pane_info_right, L"");
		window.set_text(pane_scale_right, L"");
	}
}

std::vector<ComPtr<IShellItem>> compare(Window& window, const std::vector<std::vector<ImagePair>>& pair_categories) {
	enum {
		button_swap_images = 100, button_first_pair, button_previous_pair, button_next_pair,
		button_open_folder_left, button_delete_file_left,
		button_open_folder_right, button_delete_file_right,
		button_file_new_scan, button_file_exit,
		button_scoring_visual, button_scoring_time, button_scoring_location, button_scoring_combined,
		button_filters_folder_any, button_filters_folder_different, button_filters_folder_same,
		button_filters_age_any, button_filters_age_year, button_filters_age_month, button_filters_age_week, button_filters_age_day,
		button_help_website, button_help_license
	};

	enum {
		checkmark_group_scoring, checkmark_group_folder, checkmark_group_age
	};

	// panes

	const auto mx = 12.0f;
	const auto my = 8.0f;
	const auto margin = D2D1::RectF(mx, my, mx, my);
	const auto margin_short = D2D1::RectF(mx, 0, mx, my);
	const auto margin_short_narrow = D2D1::RectF(mx, 0, 0, my);
	const auto margin_narrow = D2D1::RectF(mx, my, 0, my);

	const auto colour_pair = Colour{0xfff8f8f8};
	const auto colour_info_left = Colour{0xffe8e8e8};
	const auto colour_info_right = Colour{0xfff0f0f0};
	const auto colour_image_left = Colour{0xffb0b0b0};
	const auto colour_image_right = Colour{0xffb8b8b8};

	window.add_edge(0);
	window.add_edge(0);
	window.add_edge(1);
	window.add_edge(1);
	window.add_edge(0.5f);
	for (int i = 0; i < 7; i++)
		window.add_edge();

	window.add_pane(0, 1, 8, 9, margin, false, true, colour_pair); // pane_pair_info
	window.add_pane(8, 1, 2, 9, margin, true, true, colour_pair); // pane_pair_buttons

	window.add_pane(0, 9, 5, 10, margin_narrow, true, true, colour_info_left); // pane_info_header
	window.add_pane(5, 9, 4, 10, margin, false, true, colour_info_left); // pane_info_left
	window.add_pane(4, 9, 2, 10, margin, false, true, colour_info_right); // pane_info_right

	window.add_pane(0, 10, 5, 11, margin_short, true, true, colour_info_left); // pane_scale_header
	window.add_pane(5, 10, 6, 11, margin_short_narrow, false, true, colour_info_left); // pane_scale_left
	window.add_pane(6, 10, 4, 11, margin_short, true, true, colour_info_left); // pane_buttons_left
	window.add_pane(4, 10, 7, 11, margin_short_narrow, false, true, colour_info_right); // pane_scale_right
	window.add_pane(7, 10, 2, 11, margin_short, true, true, colour_info_right); // pane_buttons_right

	window.add_pane(0, 11, 4, 3, {0, 0, 0, 0}, false, false, colour_image_left); // pane_image_left
	window.add_pane(4, 11, 2, 3, {0, 0, 0, 0}, false, false, colour_image_right); // pane_image_right

	// buttons

	window.add_button(pane_pair_buttons, button_swap_images, L"Swap images");
	window.add_button(pane_pair_buttons, button_first_pair, L"First pair");
	window.add_button(pane_pair_buttons, button_previous_pair, L"Previous pair");
	window.add_button(pane_pair_buttons, button_next_pair, L"Next pair");

	window.add_button(pane_buttons_left, button_open_folder_left, L"Open folder");
	window.add_button(pane_buttons_left, button_delete_file_left, L"Delete file");

	window.add_button(pane_buttons_right, button_open_folder_right, L"Open folder");
	window.add_button(pane_buttons_right, button_delete_file_right, L"Delete file");

	window.set_button_focus(button_next_pair);

	// menu

	window.push_menu_level(L"File");
	window.add_menu_item(L"New scan...", button_file_new_scan);
	window.add_menu_item(L"Exit", button_file_exit);
	window.pop_menu_level();

	window.push_menu_level(L"Scoring");
	window.add_menu_item(L"Visual similarity", button_scoring_visual, checkmark_group_scoring);
	window.add_menu_item(L"Time difference (metadata)", button_scoring_time, checkmark_group_scoring);
	window.add_menu_item(L"Location distance (metadata)", button_scoring_location, checkmark_group_scoring);
	window.add_menu_item(L"Combined", button_scoring_combined, checkmark_group_scoring);
	window.pop_menu_level();

	window.push_menu_level(L"Filters");
	window.push_menu_level(L"Folder restrictions");
	window.add_menu_item(L"Images in a pair can be anywhere", button_filters_folder_any, checkmark_group_folder);
	window.add_menu_item(L"Images in a pair must be in different folders", button_filters_folder_different, checkmark_group_folder);
	window.add_menu_item(L"Images in a pair must be in the same folder", button_filters_folder_same, checkmark_group_folder);
	window.pop_menu_level();
	window.push_menu_level(L"Maximum pair age");
	window.add_menu_item(L"Unlimited", button_filters_age_any, checkmark_group_age);
	window.add_menu_item(L"One year", button_filters_age_year, checkmark_group_age);
	window.add_menu_item(L"One month", button_filters_age_month, checkmark_group_age);
	window.add_menu_item(L"One week", button_filters_age_week, checkmark_group_age);
	window.add_menu_item(L"One day", button_filters_age_day, checkmark_group_age);
	window.pop_menu_level();
	window.pop_menu_level();

	window.push_menu_level(L"Help");
	window.add_menu_item(L"Website...", button_help_website);
	window.add_menu_item(L"License...", button_help_license);
	window.pop_menu_level();

	// ui settings

	static enum class Scoring {visual, time, location, combined} scoring = Scoring::combined;
	static enum class FolderFilter {any, same, different} folder_filter = FolderFilter::any;
	static auto maximum_pair_age = std::chrono::system_clock::duration::max();

	if (scoring == Scoring::visual)
		window.set_menu_item_checked(button_scoring_visual);
	else if (scoring == Scoring::time)
		window.set_menu_item_checked(button_scoring_time);
	else if (scoring == Scoring::location)
		window.set_menu_item_checked(button_scoring_location);
	else if (scoring == Scoring::combined)
		window.set_menu_item_checked(button_scoring_combined);
	else
		assert(false);

	if (folder_filter == FolderFilter::any)
		window.set_menu_item_checked(button_filters_folder_any);
	else if (folder_filter == FolderFilter::same)
		window.set_menu_item_checked(button_filters_folder_same);
	else if (folder_filter == FolderFilter::different)
		window.set_menu_item_checked(button_filters_folder_different);
	else
		assert(false);

	if (maximum_pair_age > std::chrono::hours(365*24))
		window.set_menu_item_checked(button_filters_age_any);
	else if (maximum_pair_age == std::chrono::hours(365*24))
		window.set_menu_item_checked(button_filters_age_year);
	else if (maximum_pair_age == std::chrono::hours(30*24))
		window.set_menu_item_checked(button_filters_age_month);
	else if (maximum_pair_age == std::chrono::hours(7*24))
		window.set_menu_item_checked(button_filters_age_week);
	else if (maximum_pair_age == std::chrono::hours(24))
		window.set_menu_item_checked(button_filters_age_day);
	else
		assert(false);

	auto pairs = pair_categories[static_cast<int>(scoring)];
	auto pairs_it = pairs.begin();

	// when text is first updated, layout will change. update text
	// here so that image fit scale will work for the first pair.
	update_text(window, pairs, pairs_it);

	std::vector<std::pair<float, float>> scale_levels;

	bool pairs_valid = false;
	bool images_valid = false;
	bool scale_levels_valid = false;
	bool text_valid = false;
	bool cursor_valid = false;
	bool buttons_valid = false;

	bool swapped_state = false;

	for (;;) {
		if (!pairs_valid) {
			bool copy_all =
				folder_filter == FolderFilter::any &&
				maximum_pair_age == std::chrono::system_clock::duration::max();
			if (copy_all) {
				pairs = pair_categories[static_cast<int>(scoring)];
			} else {
				pairs.clear();

				if (folder_filter == FolderFilter::any)
					std::copy_if(
						pair_categories[static_cast<int>(scoring)].cbegin(),
						pair_categories[static_cast<int>(scoring)].cend(),
						back_inserter(pairs),
						[&](const ImagePair& d) {
							return d.get_age() < maximum_pair_age;
						});
				else if (folder_filter == FolderFilter::same)
					std::copy_if(
						pair_categories[static_cast<int>(scoring)].cbegin(),
						pair_categories[static_cast<int>(scoring)].cend(),
						back_inserter(pairs),
						[&](const ImagePair& d) {
							return d.get_age() < maximum_pair_age && d.is_in_same_folder();
						});
				else if (folder_filter == FolderFilter::different)
					std::copy_if(
						pair_categories[static_cast<int>(scoring)].cbegin(),
						pair_categories[static_cast<int>(scoring)].cend(),
						back_inserter(pairs),
						[&](const ImagePair& d) {
							return d.get_age() < maximum_pair_age && !d.is_in_same_folder();
						});
			}

			pairs_it = pairs.begin();

			auto buttons = {
				button_swap_images, button_first_pair, button_previous_pair, button_next_pair,
				button_open_folder_left, button_delete_file_left,
				button_open_folder_right, button_delete_file_right};
			if (pairs.empty())
				for (auto b : buttons)
					window.set_button_state(b, false);
			else
				for (auto b : buttons)
					window.set_button_state(b, true);

			window.set_dirty();
		}

		if (!images_valid) {
			if (pairs.empty()) {
				window.set_image(pane_image_left, nullptr);
				window.set_image(pane_image_right, nullptr);
			} else {
				window.set_image(pane_image_left, pairs_it->image_1);
				window.set_image(pane_image_right, pairs_it->image_2);
			}
			swapped_state = false;
		}

		if (!scale_levels_valid) {
			if (window.get_image(pane_image_left) && window.get_image(pane_image_right)) {
				auto bitmap_size_left = window.get_image(pane_image_left)->get_bitmap_size(window.get_scale());
				auto bitmap_size_right = window.get_image(pane_image_right)->get_bitmap_size(window.get_scale());

				auto fsl = get_fit_scale(rect_size(window.content(pane_image_left)), bitmap_size_left);
				auto fsr = get_fit_scale(rect_size(window.content(pane_image_right)), bitmap_size_right);

				// left/right scales before swap: (1, 1)
				// ratio of all left/right scales in un-swapped mode: 1
				//
				// left/right scales after swap: (1*(wl/wr), 1*(wr/wl))
				// ratio of all left/right scales in swapped mode: (wl/wr) / (wr/wl) = (wl*wl) / (wr*wr)
				if (swapped_state) {
					auto wl = bitmap_size_left.w;
					auto wr = bitmap_size_right.w;
					auto swapped_left_right_scale_ratio = (wl*wl) / (wr*wr);

					scale_levels = get_scale_levels(fsl, fsr, swapped_left_right_scale_ratio);
				} else {
					scale_levels = get_scale_levels(fsl, fsr, 1);
				}
			} else {
				scale_levels.clear();
			}
		}

		if (!images_valid) {
			if (!scale_levels.empty()) {
				window.set_image_scale(pane_image_left, scale_levels[0].first);
				window.set_image_scale(pane_image_right, scale_levels[0].second);
			}
		}

		if (!text_valid) {
			update_text(window, pairs, pairs_it);
			window.set_dirty();
		}

		if (!cursor_valid) {
			for (auto& pane : {pane_image_left, pane_image_right}) {
				bool image_wider =
					window.get_image(pane) &&
					std::floor(window.content(pane).right - window.content(pane).left) <
					std::floor(window.get_image_scale(pane) * window.get_image(pane)->get_bitmap_size(window.get_scale()).w);
				bool image_taller =
					window.get_image(pane) &&
					std::floor(window.content(pane).bottom - window.content(pane).top) <
					std::floor(window.get_image_scale(pane) * window.get_image(pane)->get_bitmap_size(window.get_scale()).h);

				if (image_wider || image_taller)
					window.set_cursor(pane, IDC_SIZEALL);
				else
					window.set_cursor(pane, IDC_ARROW);
			}
		}

		if (!buttons_valid) {
			if (!pairs.empty()) {
				window.set_button_state(button_delete_file_left, window.get_image(pane_image_left)->is_deletable());
				window.set_button_state(button_delete_file_right, window.get_image(pane_image_right)->is_deletable());
			}
		}

		pairs_valid = true;
		images_valid = true;
		scale_levels_valid = true;
		text_valid = true;
		cursor_valid = true;
		buttons_valid = true;

		auto e = window.get_event();

		if (e.type == Event::Type::button) {
			switch(e.button_id) {
			case button_next_pair:
				if (!pairs.empty()) {
					pairs_it++;
					if (pairs_it == pairs.end())
						pairs_it = pairs.begin();

					images_valid = false;
					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;
			case button_previous_pair:
				if (!pairs.empty()) {
					if (pairs_it == pairs.begin())
						pairs_it = pairs.end();
					pairs_it--;

					images_valid = false;
					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;
			case button_first_pair:
				if (!pairs.empty()) {
					pairs_it = pairs.begin();

					images_valid = false;
					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;
			case button_swap_images:
				if (window.get_image(pane_image_left) && window.get_image(pane_image_right)) {
					// swapping the images of a pair of images (1) swaps the
					// images themselves and (2) changes the scale of each image
					// so that the screen-space widths after the swap matches
					// the screen-space widths of the other image before the
					// swap (the image on the left is the same screen-space
					// width before and after the swap).

					// width_left_ss = width_left * scale_left
					// width_right_ss = width_right * scale_right
					//
					// we want to make the width of the right image in screen
					// space the width of the left image in screen space by
					// changing the scale of the right image:
					//
					// width_right * scale_right = width_left_ss <=>
					// scale_right = width_left_ss / width_right <=>
					// scale_right = (width_left * scale_left) / width_right <=>
					// scale_right = (width_left / width_right) * scale_left
					//
					// this is the new scale for the right image when we have
					// moved it to the left side. we then do the same for the
					// right image.

					// potential issue: scale values calculated here must
					// exactly match the scale values returned by
					// get_scale_levels() for zoom() to identify these scale
					// values with the correct zoom level.
					//
					// if, due to floating point precision errors, scale 0.99999
					// is calculated here and 1.00000 in get_scale_levels(),
					// zooming in will move from scale 0.99999 to scale 1.00000,
					// which is not expected by the user. the error will only
					// happen once per swap however.
					
					auto wl = window.get_image(pane_image_left)->get_bitmap_size(window.get_scale()).w;
					auto wr = window.get_image(pane_image_right)->get_bitmap_size(window.get_scale()).w;
					window.set_image_scale(pane_image_left, (wl / wr) * window.get_image_scale(pane_image_left));
					window.set_image_scale(pane_image_right, (wr / wl) * window.get_image_scale(pane_image_right));

					auto swap = window.get_image(pane_image_left);
					window.set_image(pane_image_left, window.get_image(pane_image_right));
					window.set_image(pane_image_right, swap);

					swapped_state = !swapped_state;

					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;
			case button_delete_file_left:
			case button_delete_file_right:
				{
					auto pane = e.button_id == button_delete_file_left ?
						pane_image_left : pane_image_right;
					if (window.get_image(pane)) {
						window.get_image(pane)->delete_file();
						window.set_dirty();
						cursor_valid = false;
						buttons_valid = false;
					}
				}
				break;
			case button_open_folder_left:
			case button_open_folder_right:
				{
					auto pane = e.button_id == button_open_folder_left ?
						pane_image_left : pane_image_right;
					if (window.get_image(pane))
						window.get_image(pane)->open_folder();
				}
				break;
			case button_file_new_scan:
				{
					std::vector<ComPtr<IShellItem>> browse(HWND parent);
					auto items = browse(window.get_handle());
					if (!items.empty())
						return items;
				}
				break;
			case button_file_exit:
				PostQuitMessage(0);
				break;
			case button_scoring_combined:
			case button_scoring_visual:
			case button_scoring_time:
			case button_scoring_location:
			case button_filters_folder_any:
			case button_filters_folder_same:
			case button_filters_folder_different:
			case button_filters_age_any:
			case button_filters_age_year:
			case button_filters_age_month:
			case button_filters_age_week:
			case button_filters_age_day:
				if (e.button_id == button_scoring_combined)
					scoring = Scoring::combined;
				else if (e.button_id == button_scoring_visual)
					scoring = Scoring::visual;
				else if (e.button_id == button_scoring_time)
					scoring = Scoring::time;
				else if (e.button_id == button_scoring_location)
					scoring = Scoring::location;
				else if (e.button_id == button_filters_folder_any)
					folder_filter = FolderFilter::any;
				else if (e.button_id == button_filters_folder_same)
					folder_filter = FolderFilter::same;
				else if (e.button_id == button_filters_folder_different)
					folder_filter = FolderFilter::different;
				else if (e.button_id == button_filters_age_any)
					maximum_pair_age = std::chrono::system_clock::duration::max();
				else if (e.button_id == button_filters_age_year)
					maximum_pair_age = std::chrono::hours(365*24);
				else if (e.button_id == button_filters_age_month)
					maximum_pair_age = std::chrono::hours(30*24);
				else if (e.button_id == button_filters_age_week)
					maximum_pair_age = std::chrono::hours(7*24);
				else if (e.button_id == button_filters_age_day)
					maximum_pair_age = std::chrono::hours(24);
				else
					assert(false);

				window.set_menu_item_checked(e.button_id);

				pairs_valid = false;
				images_valid = false;
				scale_levels_valid = false;
				text_valid = false;
				cursor_valid = false;
				buttons_valid = false;
				break;
			case button_help_website:
				ShellExecute(nullptr, L"open", L"https://github.com/olaolsso/pixiple/", nullptr, nullptr, SW_SHOWNORMAL);
				break;
			case button_help_license:
				{
					std::wstring license{L"The MIT License (MIT)\n\nCopyright (c) 2016 Ola Olsson\n\n"
					"Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\n"
					"The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\n"
					"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE."};
					window.message_box(license.c_str());
				}
				break;
			}
		} else if (e.type == Event::Type::drag) {
			auto pane = window.get_pane(e.drag_mouse_position_start);

			if (pane > 0 && window.get_image(pane)) {
				auto translation_isn = Vector2f{
					e.drag_mouse_position_delta.x / window.get_image(pane)->get_bitmap_size(window.get_scale()).w / window.get_image_scale(pane),
					e.drag_mouse_position_delta.y / window.get_image(pane)->get_bitmap_size(window.get_scale()).h / window.get_image_scale(pane)};
				window.translate_image_centre(pane, translation_isn);

				auto pane_other = pane == pane_image_left ? pane_image_right : pane_image_left;
				window.set_image_centre_from_other_pane(pane_other, pane);

				window.set_dirty();
			}
		} else if (e.type == Event::Type::items) {
			return e.items;
		} else if (e.type == Event::Type::key) {
			if (e.key_code == VK_NEXT || e.key_code == 'N') {
				window.click_button(button_next_pair);
			} else if (e.key_code == VK_PRIOR || e.key_code == 'P') {
				window.click_button(button_previous_pair);
			} else if (e.key_code == 'F') {
				window.click_button(button_first_pair);
			} else if (e.key_code == 'S') {
				window.click_button(button_swap_images);
			} else if (e.key_code == 'Z' || e.key_code == 'X') {
				if (!pairs.empty()) {
					zoom(window, scale_levels, e.key_code == 'Z' ? 1 : -1);
					text_valid = false;
					cursor_valid = false;
				}
			}
		} else if (e.type == Event::Type::quit) {
			return {};
		} else if (e.type == Event::Type::wheel) {
			if (!pairs.empty()) {
				zoom(window, scale_levels, e.wheel_count_delta);
				text_valid = false;
				cursor_valid = false;
			}
		} else if (e.type == Event::Type::size) {
			scale_levels_valid = false;
			text_valid = false;
			cursor_valid = false;
		} else {
			assert(false);
		}

		assert(rect_size(window.content(pane_image_left)).w == rect_size(window.content(pane_image_right)).w);
		assert(rect_size(window.content(pane_image_left)).h == rect_size(window.content(pane_image_right)).h);
	}
}
