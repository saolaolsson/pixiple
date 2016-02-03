#include "shared.h"

#include "d2d.h"
#include "duplicate.h"
#include "image.h"
#include "window.h"

#include "shared/map.h"

#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>

#include <windowsx.h>

enum {
	PANE_PAIR_INFO, PANE_PAIR_BUTTONS,
	//PANE_SCORING_HEADER, PANE_SCORING, PANE_FILTERS_HEADER, PANE_FILTERS,
	PANE_INFO_HEADER, PANE_INFO_LEFT, PANE_INFO_RIGHT,
	PANE_SCALE_HEADER, PANE_SCALE_LEFT, PANE_BUTTONS_LEFT, PANE_SCALE_RIGHT, PANE_BUTTONS_RIGHT,
	PANE_IMAGE_LEFT, PANE_IMAGE_RIGHT };

const int scale_level_exponent_min = -6;
const int scale_level_exponent_max = 6;

float get_fit_scale(const D2D1_SIZE_F pane_size, const D2D1_SIZE_F bitmap_size) {
	float fit_scale = std::min(
		pane_size.width / bitmap_size.width,
		pane_size.height / bitmap_size.height);
	return clamp(
		fit_scale,
		pow(2.0f, scale_level_exponent_min),
		pow(2.0f, scale_level_exponent_max));
}

std::vector<std::pair<float, float>> get_scale_levels(
	const float fit_scale_left,
	const float fit_scale_right,
	const float swapped_left_right_scale_ratio) {

	std::vector<std::pair<float, float>> scale_level_pairs;

	// add fit scales (with corresponding scale for other image)
	scale_level_pairs.push_back(std::make_pair(
		fit_scale_left,
		fit_scale_left * swapped_left_right_scale_ratio));
	scale_level_pairs.push_back(std::make_pair(
		fit_scale_right / swapped_left_right_scale_ratio,
		fit_scale_right));

	// find smallast scale to include
	float min_scale = std::min({
		scale_level_pairs[0].first,
		scale_level_pairs[0].second,
		scale_level_pairs[1].first,
		scale_level_pairs[1].second });
	min_scale = std::min(min_scale, 1.0f);

	// add fixed scales (with corresponding scale for other image)
	for (int i = scale_level_exponent_min; i <= scale_level_exponent_max; i++) {
		float sl = pow(2.0f, i);
		float sl1 = sl * swapped_left_right_scale_ratio;
		float sl2 = sl / swapped_left_right_scale_ratio;

		if (sl >= min_scale && sl1 >= min_scale)
			scale_level_pairs.push_back(std::make_pair(sl, sl1));
		if (sl >= min_scale && sl2 >= min_scale)
			scale_level_pairs.push_back(std::make_pair(sl2, sl));
	}

	// remove duplicates
	std::sort(scale_level_pairs.begin(), scale_level_pairs.end());
	scale_level_pairs.erase(std::unique(scale_level_pairs.begin(), scale_level_pairs.end()), scale_level_pairs.end());
	assert(!scale_level_pairs.empty());

	#ifdef _DEBUG
	// assert scales strictly ascending
	float previous_first = 0;
	float previous_second = 0;
	for (const auto& slp : scale_level_pairs) {
		assert(slp.first > previous_first);
		assert(slp.second > previous_second);
		previous_first = slp.first;
		previous_second = slp.second;
	}

	//for (const auto& slp : scale_level_pairs)
	//	debug_log << std::fixed << slp.first << " " << slp.second << std::endl;
	#endif

	return scale_level_pairs;
}

void zoom(Window& window, const std::vector<std::pair<float, float>> scale_levels, const int wheel_count_delta) {
	// find pane to zoom in

	int pane = window.get_pane(window.get_mouse_position());
	D2D1_POINT_2F zoom_point;
	if (pane > 0 && window.get_image(pane)) {
		zoom_point = D2D1::Point2F(
			window.get_mouse_position().x - (window.content(pane).left + window.content(pane).right) / 2.0f,
			window.get_mouse_position().y - (window.content(pane).top + window.content(pane).bottom) / 2.0f);
	} else {
		pane = PANE_IMAGE_LEFT;
		zoom_point = D2D1::Point2F(0, 0);
	}

	// get the new scale

	float scale;
	float scale_other;

	if (wheel_count_delta < 0) {
		// zoom out
		auto sl_i = std::find_if(scale_levels.crbegin(), scale_levels.crend(), [&](const std::pair<float, float>& scales) {
			return scales.first < window.get_image_scale(PANE_IMAGE_LEFT);
		});
		if (sl_i == scale_levels.crend())
			return;
		scale = sl_i->first;
		scale_other = sl_i->second;;
	} else {
		// zoom in
		auto sl_i = std::find_if(scale_levels.cbegin(), scale_levels.cend(), [&](const std::pair<float, float>& scales) {
			return scales.first > window.get_image_scale(PANE_IMAGE_LEFT);
		});
		if (sl_i == scale_levels.cend())
			return;
		scale = sl_i->first;
		scale_other = sl_i->second;;
	}

	if (pane == PANE_IMAGE_RIGHT)
		std::swap(scale, scale_other);

	window.image_zoom_transform(pane, scale, zoom_point);

	int pane_other = pane == PANE_IMAGE_LEFT ? PANE_IMAGE_RIGHT : PANE_IMAGE_LEFT;
	window.set_image_scale(pane_other, scale_other);
	window.set_image_centre_from_other_pane(pane_other, pane);
}

std::size_t get_matching_text_length(
	const std::wstring& text1,
	const std::wstring& text2,
	const std::size_t start1 = 0,
	const std::size_t start2 = 0) {

	assert(start1 >= 0 && start1 <= text1.length());
	assert(start2 >= 0 && start2 <= text2.length());

	std::size_t search_length = std::min(text1.length() - start1, text2.length() - start2);

	std::size_t i = 0;
	while (i < search_length && text1[start1 + i] == text2[start2 + i])
		i++;

	return i;
}

std::wstring to_string(const std::chrono::system_clock::time_point& tp) {
	std::wostringstream ss;
	ss.imbue(std::locale(""));

	auto t = std::chrono::system_clock::to_time_t(tp);
	tm tm;
	et = localtime_s(&tm, &t) == 0;
	ss << std::put_time(&tm, L"%c");

	return ss.str();
}

void update_text_image_info(
	Window& window,
	const std::shared_ptr<const Image>& image,
	const std::shared_ptr<const Image>& image_other,
	int pane,
	const float scale,
	const float fit_scale,
	int scale_pane) {

	std::wostringstream ss;
	ss.imbue(std::locale(""));

	std::vector<std::pair<std::size_t, std::size_t>> bold_ranges;

	std::size_t index = 0;

	// filename

	ss << image->get_path() << L"\n";
	std::size_t matching_length = get_matching_text_length(image->get_path(), image_other->get_path());
	bold_ranges.push_back(std::make_pair(index, matching_length));

	std::size_t name_start = image->get_path().find_last_of(L'\\');
	std::size_t name_start_other = image_other->get_path().find_last_of(L'\\');
	if (name_start != std::string::npos && name_start_other != std::string::npos) {
		name_start++;
		name_start_other++;

		matching_length = get_matching_text_length(image->get_path(), image_other->get_path(), name_start, name_start_other);
		bold_ranges.push_back(std::make_pair(index + name_start, matching_length));
	}
	index = ss.str().length();

	// file

	ss << image->get_file_size() << L" bytes, ";
	if (image->get_file_size() == image_other->get_file_size())
		bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
	index = ss.str().length();

	ss << to_string(image->get_file_time()) << L", ";
	if (image->get_file_time() == image_other->get_file_time())
		bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
	index = ss.str().length();

	ss  << L"hash " << image->get_file_hash() << L"\n";
	if (image->get_file_hash() == image_other->get_file_hash())
		bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
	index = ss.str().length();

	// pixels

	ss << image->get_image_size().width << L" \u00d7 " << image->get_image_size().height << L", ";
	if (image->get_image_size().width == image_other->get_image_size().width && image->get_image_size().height == image_other->get_image_size().height)
		bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
	index = ss.str().length();

	ss  << L"hash " << image->get_pixel_hash() << L"\n";
	if (image->get_pixel_hash() == image_other->get_pixel_hash())
		bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
	index = ss.str().length();

	// metadata times

	auto image_other_metadata_times = image_other->get_metadata_times();
	for (auto t : image->get_metadata_times()) {
		ss << to_string(t);

		auto r = std::find(image_other_metadata_times.begin(), image_other_metadata_times.end(), t);
		if (r != image_other_metadata_times.end()) {
			bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
		}

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
			bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
		ss << L", ";
		index = ss.str().length();
	}

	// metadata position

	if (image->get_metadata_position().x != 0 && image->get_metadata_position().y != 0) {
		ss << L"(" << image->get_metadata_position().y << L", " << image->get_metadata_position().x << L")";
		if (image->get_metadata_position().x == image_other->get_metadata_position().x && image->get_metadata_position().y == image_other->get_metadata_position().y)
			bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
		ss << L", ";
		index = ss.str().length();
	}

	// metadata image id

	if (!image->get_metadata_image_id().empty()) {
		ss << image->get_metadata_image_id();
		if (image->get_metadata_image_id() == image_other->get_metadata_image_id())
			bold_ranges.push_back(std::make_pair(index, ss.str().length() - index));
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
		ss << int(scale * 100.0f) << L" % of actual size";
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
	const std::vector<Duplicate>& duplicates,
	const std::vector<Duplicate>::const_iterator& duplicates_it) {

	std::wostringstream ss;
	ss.imbue(std::locale(""));
	ss << std::fixed;

	if (!duplicates.empty()) {
		ss << L"Image pair " << 1 + distance(duplicates.begin(), duplicates_it) << L" of " << duplicates.size();
		ss << std::setprecision(3);
		float score = duplicates_it->distance;
		ss << L": Distance ";
		if (score == std::numeric_limits<float>::max())
			ss << L"\u221e";
		else
			ss << score;
		//ss << std::setprecision(5);
		//ss << L" (distance = " << duplicates_it->get_distance() << ", distance_colour = " << duplicates_it->get_distance_colour() << L")";
	} else {
		ss << L"No images";
	}
	window.set_text(PANE_PAIR_INFO, ss.str());
	ss.str(L"");

	ss << L"Filename\n";
	ss << L"File\n";
	ss << L"Pixels\n";
	ss << L"Metadata";
	window.set_text(PANE_INFO_HEADER, ss.str());
	ss.str(L"");

	ss << L"Scale";
	window.set_text(PANE_SCALE_HEADER, ss.str());
	ss.str(L"");

	if (window.get_image(PANE_IMAGE_LEFT)) {
		update_text_image_info(
			window,
			window.get_image(PANE_IMAGE_LEFT),
			window.get_image(PANE_IMAGE_RIGHT),
			PANE_INFO_LEFT,
			window.get_image_scale(PANE_IMAGE_LEFT),
			get_fit_scale(
				rect_size(window.content(PANE_IMAGE_LEFT)),
				window.get_image(PANE_IMAGE_LEFT)->get_bitmap_size(window.get_scale())),
			PANE_SCALE_LEFT);
	} else {
		window.set_text(PANE_INFO_LEFT, L"");
		window.set_text(PANE_SCALE_LEFT, L"");
	}

	if (window.get_image(PANE_IMAGE_RIGHT)) {
		update_text_image_info(
			window,
			window.get_image(PANE_IMAGE_RIGHT),
			window.get_image(PANE_IMAGE_LEFT),
			PANE_INFO_RIGHT,
			window.get_image_scale(PANE_IMAGE_RIGHT),
			get_fit_scale(
				rect_size(window.content(PANE_IMAGE_RIGHT)),
				window.get_image(PANE_IMAGE_RIGHT)->get_bitmap_size(window.get_scale())),
			PANE_SCALE_RIGHT);
	} else {
		window.set_text(PANE_INFO_RIGHT, L"");
		window.set_text(PANE_SCALE_RIGHT, L"");
	}
}

void compare(Window& window, const std::vector<std::vector<Duplicate>>& duplicate_categories) {
	// panes

	enum {
		BUTTON_SWAP_IMAGES = 100, BUTTON_FIRST_PAIR, BUTTON_PREVIOUS_PAIR, BUTTON_NEXT_PAIR,
		BUTTON_OPEN_FOLDER_LEFT, BUTTON_DELETE_FILE_LEFT,
		BUTTON_OPEN_FOLDER_RIGHT, BUTTON_DELETE_FILE_RIGHT,
		BUTTON_FILE_NEW_SCAN, BUTTON_FILE_EXIT,
		BUTTON_SCORING_VISUAL, BUTTON_SCORING_TIME, BUTTON_SCORING_LOCATION, BUTTON_SCORING_COMBINED,
		BUTTON_FILTERS_FOLDER_ANY, BUTTON_FILTERS_FOLDER_DIFFERENT, BUTTON_FILTERS_FOLDER_SAME,
		BUTTON_FILTERS_AGE_ANY, BUTTON_FILTERS_AGE_YEAR, BUTTON_FILTERS_AGE_MONTH, BUTTON_FILTERS_AGE_WEEK, BUTTON_FILTERS_AGE_DAY,
		BUTTON_HELP_ABOUT };

	enum { CHECKMARK_GROUP_SCORING, CHECKMARK_GROUP_FOLDER, CHECKMARK_GROUP_AGE };

	const float mx = 12;
	const float my = 8;
	const D2D1_RECT_F margin = {mx, my, mx, my};
	const D2D1_RECT_F margin_short = {mx, 0, mx, my};
	const D2D1_RECT_F margin_short_narrow = {mx, 0, 0, my};
	const D2D1_RECT_F margin_narrow = {mx, my, 0, my};

	const D2D1_COLOR_F colour_pair = D2D1::ColorF(0xf8f8f8);
	const D2D1_COLOR_F colour_info_left = D2D1::ColorF(0xe8e8e8);
	const D2D1_COLOR_F colour_info_right = D2D1::ColorF(0xf0f0f0);
	const D2D1_COLOR_F colour_image_left = D2D1::ColorF(0xb0b0b0);
	const D2D1_COLOR_F colour_image_right = D2D1::ColorF(0xb8b8b8);

	window.add_edge(0);
	window.add_edge(0);
	window.add_edge(1);
	window.add_edge(1);
	window.add_edge(0.5f);
	for (int i = 0; i < 7; i++)
		window.add_edge();

	window.add_pane(PANE_PAIR_INFO, 0, 1, 8, 9, margin, false, true, colour_pair);
	window.add_pane(PANE_PAIR_BUTTONS, 8, 1, 2, 9, margin, true, true, colour_pair);

	#if 0
	// top edge: 12
	// left to right edges: 0 13 14 15 2 
	// bottom edge: 9
	window.add_pane(PANE_SCORING_HEADER, 0, 12, 13, 9, margin_short_narrow, true, true, colour_pair);
	window.add_pane(PANE_SCORING, 13, 12, 14, 9, margin_short, true, true, colour_pair);
	window.add_pane(PANE_FILTERS_HEADER, 14, 12, 15, 9, margin_short_narrow, false, true, colour_pair);
	window.add_pane(PANE_FILTERS, 15, 12, 2, 9, margin_short, true, true, colour_pair);
	#endif

	window.add_pane(PANE_INFO_HEADER, 0, 9, 5, 10, margin_narrow, true, true, colour_info_left);
	window.add_pane(PANE_INFO_LEFT, 5, 9, 4, 10, margin, false, true, colour_info_left);
	window.add_pane(PANE_INFO_RIGHT, 4, 9, 2, 10, margin, false, true, colour_info_right);

	window.add_pane(PANE_SCALE_HEADER, 0, 10, 5, 11, margin_short, true, true, colour_info_left);
	window.add_pane(PANE_SCALE_LEFT, 5, 10, 6, 11, margin_short_narrow, false, true, colour_info_left);
	window.add_pane(PANE_BUTTONS_LEFT, 6, 10, 4, 11, margin_short, true, true, colour_info_left);
	window.add_pane(PANE_SCALE_RIGHT, 4, 10, 7, 11, margin_short_narrow, false, true, colour_info_right);
	window.add_pane(PANE_BUTTONS_RIGHT, 7, 10, 2, 11, margin_short, true, true, colour_info_right);

	window.add_pane(PANE_IMAGE_LEFT, 0, 11, 4, 3, {0, 0, 0, 0}, false, false, colour_image_left);
	window.add_pane(PANE_IMAGE_RIGHT, 4, 11, 2, 3, {0, 0, 0, 0}, false, false, colour_image_right);

	// buttons

	window.add_button(PANE_PAIR_BUTTONS, BUTTON_SWAP_IMAGES, L"Swap images");
	window.add_button(PANE_PAIR_BUTTONS, BUTTON_FIRST_PAIR, L"First pair");
	window.add_button(PANE_PAIR_BUTTONS, BUTTON_PREVIOUS_PAIR, L"Previous pair");
	window.add_button(PANE_PAIR_BUTTONS, BUTTON_NEXT_PAIR, L"Next pair");

	window.add_button(PANE_BUTTONS_LEFT, BUTTON_OPEN_FOLDER_LEFT, L"Open folder");
	window.add_button(PANE_BUTTONS_LEFT, BUTTON_DELETE_FILE_LEFT, L"Delete file");

	window.add_button(PANE_BUTTONS_RIGHT, BUTTON_OPEN_FOLDER_RIGHT, L"Open folder");
	window.add_button(PANE_BUTTONS_RIGHT, BUTTON_DELETE_FILE_RIGHT, L"Delete file");

	window.set_button_focus(BUTTON_NEXT_PAIR);

	// menu

	window.push_menu_level(L"File");
	window.add_menu_item(L"&New scan...\tCtrl+N", BUTTON_FILE_NEW_SCAN);
	window.add_menu_item(L"Exit", BUTTON_FILE_EXIT);
	window.pop_menu_level();

	window.push_menu_level(L"Scoring");
	window.add_menu_item(L"&Visual similarity\tCtrl+V", BUTTON_SCORING_VISUAL, CHECKMARK_GROUP_SCORING);
	window.add_menu_item(L"&Time difference (metadata)\tCtrl+T", BUTTON_SCORING_TIME, CHECKMARK_GROUP_SCORING);
	window.add_menu_item(L"&Location distance (metadata)\tCtrl+L", BUTTON_SCORING_LOCATION, CHECKMARK_GROUP_SCORING);
	window.add_menu_item(L"&Combined\tCtrl+C", BUTTON_SCORING_COMBINED, CHECKMARK_GROUP_SCORING);
	window.pop_menu_level();

	window.push_menu_level(L"Filters");
	window.push_menu_level(L"Folder restrictions");
	window.add_menu_item(L"Images in a pair can be anywhere", BUTTON_FILTERS_FOLDER_ANY, CHECKMARK_GROUP_FOLDER);
	window.add_menu_item(L"Images in a pair must be in different folders", BUTTON_FILTERS_FOLDER_DIFFERENT, CHECKMARK_GROUP_FOLDER);
	window.add_menu_item(L"Images in a pair must be in the same folder", BUTTON_FILTERS_FOLDER_SAME, CHECKMARK_GROUP_FOLDER);
	window.pop_menu_level();
	window.push_menu_level(L"Maximum pair age");
	window.add_menu_item(L"Unlimited", BUTTON_FILTERS_AGE_ANY, CHECKMARK_GROUP_AGE);
	window.add_menu_item(L"One year", BUTTON_FILTERS_AGE_YEAR, CHECKMARK_GROUP_AGE);
	window.add_menu_item(L"One month", BUTTON_FILTERS_AGE_MONTH, CHECKMARK_GROUP_AGE);
	window.add_menu_item(L"One week", BUTTON_FILTERS_AGE_WEEK, CHECKMARK_GROUP_AGE);
	window.add_menu_item(L"One day", BUTTON_FILTERS_AGE_DAY, CHECKMARK_GROUP_AGE);
	window.pop_menu_level();
	window.pop_menu_level();

	window.push_menu_level(L"Help");
	window.add_menu_item(L"About Pixiple...", BUTTON_HELP_ABOUT);
	window.pop_menu_level();

	#if 0
	window.set_text(PANE_SCORING_HEADER, L"Similarity scoring");
	window.set_text(PANE_FILTERS_HEADER, L"Filters");

	window.add_combobox(
		PANE_SCORING, BUTTON_TEST, {
		L"Combined",
		L"Visual",
		L"Metadata time difference",
		L"Metadata positioning distance"});

	window.add_combobox(
		PANE_FILTERS, BUTTON_TEST, {
		L"Minimum score threshold: Exact",
		L"Minimum score threshold: Excellent",
		L"Minimum score threshold: Good",
		L"Minimum score threshold: Fair",
		L"Minimum score threshold: Poor" });

	window.add_combobox(
		PANE_FILTERS, BUTTON_TEST, {
		L"Folder: pair images can be anywhere",
		L"Folder: pair images must be in different folders",
		L"Folder: pair images must be in the same folder"});

	window.add_combobox(
		PANE_FILTERS, BUTTON_TEST, {
		L"Maximum pair age: unlimited",
		L"Maximum pair age: one year",
		L"Maximum pair age: one month",
		L"Maximum pair age: one week",
		L"Maximum pair age: one day"});
	#endif

	bool duplicates_valid = false;
	bool images_valid = false;
	bool scale_levels_valid = false;
	bool text_valid = false;
	bool cursor_valid = false;
	bool buttons_valid = false;

	bool swapped_state = false;

	enum Scoring { SCORING_VISUAL, SCORING_TIME, SCORING_LOCATION, SCORING_COMBINED, SCORING_N }
		scoring = SCORING_COMBINED;
	assert(duplicate_categories.size() == SCORING_N);
	window.set_menu_item_checked(BUTTON_SCORING_COMBINED);

	enum FolderFilter { FOLDER_FILTER_ANY, FOLDER_FILTER_SAME, FOLDER_FILTER_DIFFERENT }
		folder_filter = FOLDER_FILTER_ANY;
	window.set_menu_item_checked(BUTTON_FILTERS_FOLDER_ANY);

	std::chrono::system_clock::duration maximum_pair_age =
		std::chrono::system_clock::duration::max();
	window.set_menu_item_checked(BUTTON_FILTERS_AGE_ANY);

	auto duplicates = duplicate_categories[scoring];
	auto duplicates_it = duplicates.begin();

	// when text is first updated, layout will change. update
	// text here so that image fit scale will work for the first pair.
	update_text(window, duplicates, duplicates_it);

	std::vector<std::pair<float, float>> scale_levels;

	for (;;) {
		if (!duplicates_valid) {
			bool copy_all =
				folder_filter == FOLDER_FILTER_ANY &&
				maximum_pair_age == std::chrono::system_clock::duration::max();
			if (copy_all) {
				duplicates = duplicate_categories[scoring];
			} else {
				duplicates.clear();

				if (folder_filter == FOLDER_FILTER_ANY)
					std::copy_if(
						duplicate_categories[scoring].cbegin(),
						duplicate_categories[scoring].cend(),
						back_inserter(duplicates),
						[&](const Duplicate& d) {
							return d.get_age() < maximum_pair_age; });
				else if (folder_filter == FOLDER_FILTER_SAME)
					std::copy_if(
						duplicate_categories[scoring].cbegin(),
						duplicate_categories[scoring].cend(),
						back_inserter(duplicates),
						[&](const Duplicate& d) {
							return d.get_age() < maximum_pair_age && d.is_in_same_folder(); });
				else if (folder_filter == FOLDER_FILTER_DIFFERENT)
					std::copy_if(
						duplicate_categories[scoring].cbegin(),
						duplicate_categories[scoring].cend(),
						back_inserter(duplicates),
						[&](const Duplicate& d) {
							return d.get_age() < maximum_pair_age && !d.is_in_same_folder(); });
			}

			duplicates_it = duplicates.begin();

			auto buttons = {
				BUTTON_SWAP_IMAGES, BUTTON_FIRST_PAIR, BUTTON_PREVIOUS_PAIR, BUTTON_NEXT_PAIR,
				BUTTON_OPEN_FOLDER_LEFT, BUTTON_DELETE_FILE_LEFT,
				BUTTON_OPEN_FOLDER_RIGHT, BUTTON_DELETE_FILE_RIGHT};
			if (duplicates.empty())
				for (int b : buttons)
					window.set_button_state(b, false);
			else
				for (int b : buttons)
					window.set_button_state(b, true);

			window.set_dirty();
		}

		if (!images_valid) {
			if (duplicates.empty()) {
				window.set_image(PANE_IMAGE_LEFT, nullptr);
				window.set_image(PANE_IMAGE_RIGHT, nullptr);
			} else {
				window.set_image(PANE_IMAGE_LEFT, duplicates_it->image_1);
				window.set_image(PANE_IMAGE_RIGHT, duplicates_it->image_2);
			}
			swapped_state = false;
		}

		if (!scale_levels_valid) {
			if (window.get_image(PANE_IMAGE_LEFT) && window.get_image(PANE_IMAGE_RIGHT)) {
				auto bitmap_size_left = window.get_image(PANE_IMAGE_LEFT)->get_bitmap_size(window.get_scale());
				auto bitmap_size_right = window.get_image(PANE_IMAGE_RIGHT)->get_bitmap_size(window.get_scale());

				float fsl = get_fit_scale(rect_size(window.content(PANE_IMAGE_LEFT)), bitmap_size_left);
				float fsr = get_fit_scale(rect_size(window.content(PANE_IMAGE_RIGHT)), bitmap_size_right);

				// left/right scales before swap: (1, 1)
				// ratio of all left/right scales in un-swapped mode: 1
				//
				// left/right scales after swap: (1*(wl/wr), 1*(wr/wl))
				// ratio of all left/right scales in swapped mode: (wl/wr) / (wr/wl) = (wl*wl) / (wr*wr)
				if (swapped_state) {
					float wl = bitmap_size_left.width;
					float wr = bitmap_size_right.width;
					float swapped_left_right_scale_ratio = (wl*wl) / (wr*wr);

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
				window.set_image_scale(PANE_IMAGE_LEFT, scale_levels[0].first);
				window.set_image_scale(PANE_IMAGE_RIGHT, scale_levels[0].second);
			}
		}

		if (!text_valid) {
			update_text(window, duplicates, duplicates_it);
			window.set_dirty();
		}

		if (!cursor_valid) {
			for (auto& pane : {PANE_IMAGE_LEFT, PANE_IMAGE_RIGHT}) {
				bool image_wider =
					window.get_image(pane) &&
					std::floor(window.content(pane).right - window.content(pane).left) <
					std::floor(window.get_image_scale(pane) * window.get_image(pane)->get_bitmap_size(window.get_scale()).width);
				bool image_taller =
					window.get_image(pane) &&
					std::floor(window.content(pane).bottom - window.content(pane).top) <
					std::floor(window.get_image_scale(pane) * window.get_image(pane)->get_bitmap_size(window.get_scale()).height);

				if (image_wider || image_taller)
					window.set_cursor(pane, IDC_SIZEALL);
				else
					window.set_cursor(pane, IDC_ARROW);
			}
		}

		if (!buttons_valid) {
			if (!duplicates.empty()) {
				window.set_button_state(BUTTON_DELETE_FILE_LEFT, window.get_image(PANE_IMAGE_LEFT)->is_deletable());
				window.set_button_state(BUTTON_DELETE_FILE_RIGHT, window.get_image(PANE_IMAGE_RIGHT)->is_deletable());
			}
		}

		duplicates_valid = true;
		images_valid = true;
		scale_levels_valid = true;
		text_valid = true;
		cursor_valid = true;
		buttons_valid = true;

		Event e = window.get_event();

		if (e.type == Event::Type::button) {
			switch(e.button_id) {
			case BUTTON_NEXT_PAIR:
				if (!duplicates.empty()) {
					duplicates_it++;
					if (duplicates_it == duplicates.end())
						duplicates_it = duplicates.begin();

					images_valid = false;
					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;

			case BUTTON_PREVIOUS_PAIR:
				if (!duplicates.empty()) {
					if (duplicates_it == duplicates.begin())
						duplicates_it = duplicates.end();
					duplicates_it--;

					images_valid = false;
					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;

			case BUTTON_FIRST_PAIR:
				if (!duplicates.empty()) {
					duplicates_it = duplicates.begin();

					images_valid = false;
					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;

			case BUTTON_SWAP_IMAGES:
				if (window.get_image(PANE_IMAGE_LEFT) && window.get_image(PANE_IMAGE_RIGHT)) {
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
					
					float wl = window.get_image(PANE_IMAGE_LEFT)->get_bitmap_size(window.get_scale()).width;
					float wr = window.get_image(PANE_IMAGE_RIGHT)->get_bitmap_size(window.get_scale()).width;
					window.set_image_scale(PANE_IMAGE_LEFT, (wl / wr) * window.get_image_scale(PANE_IMAGE_LEFT));
					window.set_image_scale(PANE_IMAGE_RIGHT, (wr / wl) * window.get_image_scale(PANE_IMAGE_RIGHT));

					auto swap = window.get_image(PANE_IMAGE_LEFT);
					window.set_image(PANE_IMAGE_LEFT, window.get_image(PANE_IMAGE_RIGHT));
					window.set_image(PANE_IMAGE_RIGHT, swap);

					swapped_state = !swapped_state;

					scale_levels_valid = false;
					text_valid = false;
					cursor_valid = false;
					buttons_valid = false;
				}
				break;

			case BUTTON_DELETE_FILE_LEFT:
			case BUTTON_DELETE_FILE_RIGHT:
				{
					int pane = e.button_id == BUTTON_DELETE_FILE_LEFT ?
						PANE_IMAGE_LEFT : PANE_IMAGE_RIGHT;
					if (window.get_image(pane)) {
						window.get_image(pane)->delete_file();
						window.set_dirty();
						cursor_valid = false;
						buttons_valid = false;
					}
				}
				break;

			case BUTTON_OPEN_FOLDER_LEFT:
			case BUTTON_OPEN_FOLDER_RIGHT:
				{
					int pane = e.button_id == BUTTON_OPEN_FOLDER_LEFT ?
						PANE_IMAGE_LEFT : PANE_IMAGE_RIGHT;
					if (window.get_image(pane))
						window.get_image(pane)->open_folder();
				}
				break;

			case BUTTON_FILE_NEW_SCAN:
				return;
				break;

			case BUTTON_FILE_EXIT:
				PostQuitMessage(0);
				break;

			case BUTTON_SCORING_COMBINED:
			case BUTTON_SCORING_VISUAL:
			case BUTTON_SCORING_TIME:
			case BUTTON_SCORING_LOCATION:
			case BUTTON_FILTERS_FOLDER_ANY:
			case BUTTON_FILTERS_FOLDER_SAME:
			case BUTTON_FILTERS_FOLDER_DIFFERENT:
			case BUTTON_FILTERS_AGE_ANY:
			case BUTTON_FILTERS_AGE_YEAR:
			case BUTTON_FILTERS_AGE_MONTH:
			case BUTTON_FILTERS_AGE_WEEK:
			case BUTTON_FILTERS_AGE_DAY:
				if (e.button_id == BUTTON_SCORING_COMBINED)
					scoring = SCORING_COMBINED;
				else if (e.button_id == BUTTON_SCORING_VISUAL)
					scoring = SCORING_VISUAL;
				else if (e.button_id == BUTTON_SCORING_TIME)
					scoring = SCORING_TIME;
				else if (e.button_id == BUTTON_SCORING_LOCATION)
					scoring = SCORING_LOCATION;
				else if (e.button_id == BUTTON_FILTERS_FOLDER_ANY)
					folder_filter = FOLDER_FILTER_ANY;
				else if (e.button_id == BUTTON_FILTERS_FOLDER_SAME)
					folder_filter = FOLDER_FILTER_SAME;
				else if (e.button_id == BUTTON_FILTERS_FOLDER_DIFFERENT)
					folder_filter = FOLDER_FILTER_DIFFERENT;
				else if (e.button_id == BUTTON_FILTERS_AGE_ANY)
					maximum_pair_age = std::chrono::system_clock::duration::max();
				else if (e.button_id == BUTTON_FILTERS_AGE_YEAR)
					maximum_pair_age = std::chrono::seconds(86400*366);
				else if (e.button_id == BUTTON_FILTERS_AGE_MONTH)
					maximum_pair_age = std::chrono::seconds(86400*31);
				else if (e.button_id == BUTTON_FILTERS_AGE_WEEK)
					maximum_pair_age = std::chrono::seconds(86400*7);
				else if (e.button_id == BUTTON_FILTERS_AGE_DAY)
					maximum_pair_age = std::chrono::seconds(86400);
				else
					assert(false);

				window.set_menu_item_checked(e.button_id);

				duplicates_valid = false;
				images_valid = false;
				scale_levels_valid = false;
				text_valid = false;
				cursor_valid = false;
				buttons_valid = false;
				break;

			default:
				debug_log << L"Unknown Event::BUTTON " << e.button_id << std::endl;
				break;
			}
		} else if (e.type == Event::Type::drag) {
			int pane = window.get_pane(e.drag_mouse_position_start);

			if (pane > 0 && window.get_image(pane)) {
				D2D1_POINT_2F translation_isn = D2D1::Point2F(
					e.drag_mouse_position_delta.x / window.get_image(pane)->get_bitmap_size(window.get_scale()).width / window.get_image_scale(pane),
					e.drag_mouse_position_delta.y / window.get_image(pane)->get_bitmap_size(window.get_scale()).height / window.get_image_scale(pane));
				window.translate_image_centre(pane, translation_isn);

				int pane_other = pane == PANE_IMAGE_LEFT ? PANE_IMAGE_RIGHT : PANE_IMAGE_LEFT;
				window.set_image_centre_from_other_pane(pane_other, pane);

				window.set_dirty();
			}
		} else if (e.type == Event::Type::key) {
			if (e.key_code == VK_NEXT || e.key_code == 'N') {
				window.click_button(BUTTON_NEXT_PAIR);
			} else if (e.key_code == VK_PRIOR || e.key_code == 'P') {
				window.click_button(BUTTON_PREVIOUS_PAIR);
			} else if (e.key_code == 'F') {
				window.click_button(BUTTON_FIRST_PAIR);
			} else if (e.key_code == 'S') {
				window.click_button(BUTTON_SWAP_IMAGES);
			} else if (e.key_code == 'Z' || e.key_code == 'X') {
				if (!duplicates.empty()) {
					zoom(window, scale_levels, e.key_code == 'Z' ? 1 : -1);
					text_valid = false;
					cursor_valid = false;
				}
			} else {
				debug_log << L"unknown key: " << e.key_code << std::endl;
			}
		} else if (e.type == Event::Type::quit) {
			return;
		} else if (e.type == Event::Type::wheel) {
			if (!duplicates.empty()) {
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

		assert(rect_size(window.content(PANE_IMAGE_LEFT)).width == rect_size(window.content(PANE_IMAGE_RIGHT)).width);
		assert(rect_size(window.content(PANE_IMAGE_LEFT)).height == rect_size(window.content(PANE_IMAGE_RIGHT)).height);
	}
}
