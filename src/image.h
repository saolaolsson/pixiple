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

struct Intensity {
	float r;
	float g;
	float b;
};
using IntensityArray = std::array<std::array<Intensity, 8>, 8>;

enum class ImageTransform {
	none, rotate_90, rotate_180, rotate_270,
	flip_h, flip_v, flip_nw_se, flip_sw_ne,
};

class Image : public std::enable_shared_from_this<Image> {
public:
	static void clear_cache();

	Image(const std::filesystem::path& path);

	enum class Status {ok, open_failed, decode_failed};
	Status get_status() const;

	std::filesystem::path path() const;
	std::uintmax_t file_size() const;
	std::experimental::filesystem::file_time_type file_time() const;

	std::vector<std::chrono::system_clock::time_point> get_metadata_times() const;
	std::wstring get_metadata_make_model() const;
	std::wstring get_metadata_camera_id() const;
	std::wstring get_metadata_image_id() const;
	Point2f get_metadata_position() const;

	Size2u get_image_size() const;
	Size2f get_bitmap_size(const Vector2f& scale) const;

	Hash get_file_hash() const;
	Hash get_pixel_hash() const;

	void draw(ID2D1HwndRenderTarget* const render_target, const D2D1_RECT_F& rect_dest, const D2D1_RECT_F& rect_src, const D2D1_BITMAP_INTERPOLATION_MODE& interpolation_mode) const;

	bool is_deletable() const;
	void delete_file() const;
	void open_folder() const;

	friend float distance(const Image& image_1, const Image& image_2, const float maximum_distance, bool& aspect_ratio_flipped, bool& cropped);

private:
	IntensityArray calculate_intensities(const std::vector<uint8_t>& pixel_buffer, const int pixel_stride, const int line_stride, const D2D_RECT_U& rect) const;

	void load_pixels(IWICBitmapFrameDecode* const frame);
	void load_metadata(IWICBitmapFrameDecode* const frame);
	void calculate_hash() const;

	ComPtr<IWICBitmapFrameDecode> get_frame(std::vector<std::uint8_t>& buffer) const;
	ComPtr<ID2D1Bitmap> get_bitmap(ID2D1HwndRenderTarget* const render_target) const;

	struct BitmapCacheEntry {
		std::weak_ptr<const Image> image;
		ComPtr<ID2D1Bitmap> bitmap = nullptr;
	};
	static std::vector<BitmapCacheEntry> bitmap_cache;
	static std::mutex bitmap_cache_mutex;

	Status status = Status::ok;

	std::filesystem::path path_;
	std::experimental::filesystem::file_time_type file_time_;

	Size2u image_size{0, 0};

	IntensityArray intensities;
	IntensityArray intensities_cropped_1;
	IntensityArray intensities_cropped_2;

	std::vector<std::chrono::system_clock::time_point> metadata_times;
	std::wstring metadata_make_model;
	std::wstring metadata_camera_id;
	std::wstring metadata_image_id;
	Point2f metadata_position{0, 0};
	
	mutable Hash file_hash;
	mutable Hash pixel_hash;
};
