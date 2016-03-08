#pragma once

#include "hash.h"

#include "shared/com.h"
#include "shared/vector.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include <d2d1.h>
#include <wincodec.h>

class Image : public std::enable_shared_from_this<Image> {
public:
	static void clear_cache();

	Image(const std::tr2::sys::path& path);
	~Image();

	enum class Status {ok, open_failed, decode_failed};
	Status get_status() const;

	std::tr2::sys::path get_path() const;
	std::uintmax_t get_file_size() const;
	std::tr2::sys::file_time_type get_file_time() const;

	std::vector<std::chrono::system_clock::time_point> get_metadata_times() const;
	std::wstring get_metadata_make_model() const;
	std::wstring get_metadata_camera_id() const;
	std::wstring get_metadata_image_id() const;
	Point2f get_metadata_position() const;

	Size2u get_image_size() const;
	Size2f get_bitmap_size(const Vector2f& scale) const;

	Hash get_file_hash() const;
	Hash get_pixel_hash() const;

	float get_distance(const Image& image, const float maximum_distance = std::numeric_limits<float>::max()) const;

	void draw(ID2D1HwndRenderTarget* const render_target, const D2D1_RECT_F& rect_dest, const D2D1_RECT_F& rect_src, const D2D1_BITMAP_INTERPOLATION_MODE& interpolation_mode) const;

	bool is_deletable() const;
	void delete_file() const;
	void open_folder() const;

private:
	static std::mutex class_mutex;
	static int n_instances;

	struct BitmapCacheEntry {
		std::weak_ptr<Image> image;
		ComPtr<ID2D1Bitmap> bitmap = nullptr;
	};
	static std::vector<BitmapCacheEntry> bitmap_cache;

	static const int n_intensity_block_divisions = 8;

	Status status = Status::ok;

	std::tr2::sys::path path;
	std::tr2::sys::file_time_type file_time;
	std::uintmax_t file_size = 0;

	Size2u image_size{0, 0};

	struct Colour {
		float r;
		float g;
		float b;
	} intensities[n_intensity_block_divisions][n_intensity_block_divisions]{0};

	std::vector<std::chrono::system_clock::time_point> metadata_times;
	std::wstring metadata_make_model;
	std::wstring metadata_camera_id;
	std::wstring metadata_image_id;
	Point2f metadata_position{0, 0};
	
	Hash file_hash;
	Hash pixel_hash;

	void load_pixels(ComPtr<IWICBitmapFrameDecode> frame);
	void load_metadata(ComPtr<IWICBitmapFrameDecode> frame);
	void calculate_hash();

	ComPtr<IWICBitmapFrameDecode> get_frame(std::vector<std::uint8_t>& buffer) const;
	ComPtr<ID2D1Bitmap> get_bitmap(ID2D1HwndRenderTarget* const render_target) const;

	enum class Transform {
		none, rotate_90, rotate_180, rotate_270,
		flip_h, flip_v, flip_nw_se, flip_sw_ne,
	};
	Colour get_intensity(const int x, const int y, const Transform transform = Transform::none) const;
};
