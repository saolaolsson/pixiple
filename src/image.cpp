#include "shared.h"

#include "image.h"

#include "shared/numeric_cast.h"
#include "shared/trim.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include <propvarutil.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <wincodec.h>

std::vector<Image::BitmapCacheEntry> Image::bitmap_cache;
std::mutex Image::bitmap_cache_mutex;

void Image::clear_cache() {
	bitmap_cache.clear();
}

Image::Image(const std::filesystem::path& path) : path_{path} {
	assert(!path.empty());

	std::error_code ec;
	// TODO: remove experimental workaround
	file_time_ = std::experimental::filesystem::last_write_time(std::experimental::filesystem::path(path.native()), ec);

	std::vector<std::uint8_t> data(numeric_cast<std::size_t>(file_size()));
	
	if (auto frame = get_frame(data)) {
		load_pixels(frame);
		load_metadata(frame);
	} else {
		status = Status::open_failed;
	}
}

Image::Status Image::get_status() const {
	return status;
}

std::filesystem::path Image::path() const {
	return path_;
}

std::uintmax_t Image::file_size() const {
	std::error_code ec;
	auto s = std::filesystem::file_size(path_, ec);
	if (s == -1)
		return 0;
	else
		return s;
}

std::experimental::filesystem::file_time_type Image::file_time() const {
	return file_time_;
}

std::vector<std::chrono::system_clock::time_point> Image::get_metadata_times() const {
	return metadata_times;
}

std::wstring Image::get_metadata_make_model() const {
	return metadata_make_model;
}

std::wstring Image::get_metadata_camera_id() const {
	return metadata_camera_id;
}

std::wstring Image::get_metadata_image_id() const {
	return metadata_image_id;
}

Point2f Image::get_metadata_position() const {
	return metadata_position;
}

Size2u Image::get_image_size() const {
	return image_size;
}

Size2f Image::get_bitmap_size(const Vector2f& scale) const {
	return {image_size.w / scale.x, image_size.h / scale.y};
}

Hash Image::get_file_hash() const {
	if (file_hash == Hash{})
		calculate_hash();
	return file_hash;
}

Hash Image::get_pixel_hash() const {
	if (pixel_hash == Hash{})
		calculate_hash();
	return pixel_hash;
}

void Image::draw(
	ID2D1HwndRenderTarget* const render_target,
	const D2D1_RECT_F& rect_dest,
	const D2D1_RECT_F& rect_src,
	const D2D1_BITMAP_INTERPOLATION_MODE& interpolation_mode
) const {
	if (auto bitmap = get_bitmap(render_target)) {
		render_target->DrawBitmap(bitmap, rect_dest, 1.0, interpolation_mode, rect_src);
	} else {
		// draw placeholder

		auto aam = render_target->GetAntialiasMode();
		render_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

		ComPtr<ID2D1SolidColorBrush> brush;
		er = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush);
		brush->SetOpacity(1.0f / 16);

		render_target->FillRectangle(rect_dest, brush);

		const auto line_offset = Point2f{32, 32};
		const auto square_offset = Point2f{64, 64};
		const auto thickness = 12.0f;

		auto centre = Point2f{
			rect_dest.left + (rect_dest.right - rect_dest.left) / 2,
			rect_dest.top + (rect_dest.bottom - rect_dest.top) / 2};

		er = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray), &brush);
		render_target->DrawRectangle(
			{centre.x - square_offset.x, centre.y - square_offset.y,
			 centre.x + square_offset.x, centre.y + square_offset.y}, brush, thickness);
		render_target->DrawLine(
			{centre.x - line_offset.x, centre.y - line_offset.y},
			{centre.x + line_offset.x, centre.y + line_offset.y}, brush, thickness);
		render_target->DrawLine(
			{centre.x + line_offset.x, centre.y - line_offset.y},
			{centre.x - line_offset.x, centre.y + line_offset.y}, brush, thickness);

		render_target->SetAntialiasMode(aam);
	}

	#if 0
	for (auto y = 0; y < intensities.size(); y++) {
		for (auto x = 0; x < intensities.size(); x++) {
			ComPtr<ID2D1SolidColorBrush> brush;
			er = render_target->CreateSolidColorBrush(D2D1::ColorF(intensities[y][x].r, intensities[y][x].g, intensities[y][x].b), &brush);
			const auto pixel_side = 16.0f;
			render_target->FillRectangle({
				rect_dest.left + (x+0)*pixel_side, rect_dest.top + (y+0)*pixel_side,
				rect_dest.left + (x+1)*pixel_side, rect_dest.top + (y+1)*pixel_side}, brush);
		}
	}
	#endif
}

static std::wstring to_windows_path(const std::filesystem::path& path) {
	if (path.wstring().substr(0, 2) == L"\\\\")
		return L"\\\\?\\UNC\\" + path.wstring().substr(2);
	else
		return L"\\\\?\\" + path.wstring();
}

bool Image::is_deletable() const {
	auto h = CreateFile(
		to_windows_path(path_).c_str(), DELETE, FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h != INVALID_HANDLE_VALUE)
		er = CloseHandle(h);
	return h != INVALID_HANDLE_VALUE;
}

void Image::delete_file() const {
	if (path_.empty())
		return;

	ComPtr<IShellItem> file;
	auto hr = SHCreateItemFromParsingName(path_.c_str(), nullptr, IID_PPV_ARGS(&file));
	if (FAILED(hr))
		return;

	clear_cache();

	ComPtr<IFileOperation> fo;
	er = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo));

	er = fo->SetOperationFlags(FOF_ALLOWUNDO | FOF_FILESONLY | FOF_NORECURSION);
	er = fo->DeleteItem(file, nullptr);

	hr = fo->PerformOperations();
	BOOL aborted;
	er = fo->GetAnyOperationsAborted(&aborted);
	if (!aborted)
		er = hr;
}

// Try to open explorer window at containing folder with file
// selected, or try to open containing folder, or fail silently.
void Image::open_folder() const {
	__unaligned auto folder = ILCreateFromPath(path_.parent_path().c_str());
	if (folder == nullptr)
		return;

	if (__unaligned auto file = ILCreateFromPath(path_.c_str())) {
		__unaligned const ITEMIDLIST* selection[]{file};
		er = SHOpenFolderAndSelectItems(folder, 1, selection, 0);
		CoTaskMemFree(file);
	} else {
		__unaligned const ITEMIDLIST* selection[]{folder};
		er = SHOpenFolderAndSelectItems(folder, 1, selection, 0);
	}

	CoTaskMemFree(folder);
}

Intensity get_intensity(const IntensityArray& intensities, const int x, const int y, const ImageTransform transform) {
	const int n_intensity_block_divisions = numeric_cast<int>(intensities.size());
	
	auto xt = x;
	auto yt = y;

	switch (transform) {
	case ImageTransform::none:
		break;
	case ImageTransform::rotate_90:
		xt = n_intensity_block_divisions - 1 - y;
		yt = x;
		break;
	case ImageTransform::rotate_180:
		xt = n_intensity_block_divisions - 1 - x;
		yt = n_intensity_block_divisions - 1 - y;
		break;
	case ImageTransform::rotate_270:
		xt = y;
		yt = n_intensity_block_divisions - 1 - x;
		break;
	case ImageTransform::flip_h:
		xt = n_intensity_block_divisions - 1 - x;
		yt = y;
		break;
	case ImageTransform::flip_v:
		xt = x;
		yt = n_intensity_block_divisions - 1 - y;
		break;
	case ImageTransform::flip_nw_se:
		xt = y;
		yt = x;
		break;
	case ImageTransform::flip_sw_ne:
		xt = n_intensity_block_divisions - 1 - y;
		yt = n_intensity_block_divisions - 1 - x;
		break;
	default:
		assert(false);
	}

	return intensities[yt][xt];
}

float calculate_distance(
	const IntensityArray& intensities_1,
	const IntensityArray& intensities_2,
	const float maximum_distance,
	bool& aspect_ratio_flipped
) {
	ImageTransform transforms[] {
		ImageTransform::none,
		ImageTransform::rotate_90,
		ImageTransform::rotate_180,
		ImageTransform::rotate_270,
		ImageTransform::flip_h,
		ImageTransform::flip_v,
		ImageTransform::flip_nw_se,
		ImageTransform::flip_sw_ne,
	};

	const auto n_intensity_block_divisions = numeric_cast<int>(intensities_1.size());
	auto sum = maximum_distance * n_intensity_block_divisions * n_intensity_block_divisions;
	for (auto t : transforms) {
		auto s = 0.0f;
		for (int y = 0; y < n_intensity_block_divisions; y++) {
			for (int x = 0; x < n_intensity_block_divisions; x++) {
				auto c1 = get_intensity(intensities_1, x, y, ImageTransform::none);
				auto c2 = get_intensity(intensities_2, x, y, t);
				s += std::abs(c2.r - c1.r) + std::abs(c2.g - c1.g) + std::abs(c2.b - c1.b);
			}
			if (s > sum)
				break;
		}
		if (s < sum) {
			sum = s;
			aspect_ratio_flipped = t == ImageTransform::rotate_90 || t == ImageTransform::rotate_270 || t == ImageTransform::flip_nw_se || t == ImageTransform::flip_sw_ne;
		}
	}
	assert(sum == sum);
	return sum / n_intensity_block_divisions / n_intensity_block_divisions;
}

float distance(
	const Image& image_1,
	const Image& image_2,
	const float maximum_distance,
	bool& aspect_ratio_flipped,
	bool& cropped
) {
	aspect_ratio_flipped = false;
	cropped = false;

	if (image_1.status != Image::Status::ok || image_2.status != Image::Status::ok)
		return std::numeric_limits<float>::max();

	auto distance = calculate_distance(image_1.intensities, image_2.intensities, maximum_distance, aspect_ratio_flipped);

	std::vector<std::pair<const IntensityArray&, const IntensityArray&>> pairs{
		{image_1.intensities_cropped_1, image_2.intensities_cropped_1},
		{image_1.intensities_cropped_1, image_2.intensities_cropped_2},
		{image_1.intensities_cropped_2, image_2.intensities_cropped_2},
	};
	for (const auto& p : pairs) {
		bool arf;
		auto d = calculate_distance(p.first, p.second, maximum_distance, arf);
		if (d < distance) {
			distance = d;
			aspect_ratio_flipped = arf;
			cropped = true;
		}
	}

	return distance;
}

IntensityArray Image::calculate_intensities(
	const std::vector<uint8_t>& pixel_buffer,
	const int pixel_stride,
	const int line_stride,
	const D2D_RECT_U& rect
) const {
	IntensityArray intensities;
	const Size2u size{rect.right - rect.left, rect.bottom - rect.top};
	const auto n_intensity_block_divisions = intensities.size();

	bool rgb_content = false;
	bool a_content = false;
	float alpha[n_intensity_block_divisions][n_intensity_block_divisions];

	for (auto by = 0; by < n_intensity_block_divisions; by++) {
		for (auto bx = 0; bx < n_intensity_block_divisions; bx++) {
			const auto offset_x = rect.left + size.w * (bx + 0) / n_intensity_block_divisions;
			const auto offset_y = rect.top + size.h * (by + 0) / n_intensity_block_divisions;
			const auto offset_x_next = rect.left + size.w * (bx + 1) / n_intensity_block_divisions;
			const auto offset_y_next = rect.top + size.h * (by + 1) / n_intensity_block_divisions;

			std::uint32_t r = 0;
			std::uint32_t g = 0;
			std::uint32_t b = 0;
			std::uint32_t a = 0;

			for (auto y = offset_y; y < offset_y_next; y++) {
				for (auto x = offset_x; x < offset_x_next; x++) {
					b += pixel_buffer[y*line_stride + x*pixel_stride + 0];
					g += pixel_buffer[y*line_stride + x*pixel_stride + 1];
					r += pixel_buffer[y*line_stride + x*pixel_stride + 2];
					a += pixel_buffer[y*line_stride + x*pixel_stride + 3];
				}
			}

			intensities[by][bx].r = static_cast<float>(r);
			intensities[by][bx].g = static_cast<float>(g);
			intensities[by][bx].b = static_cast<float>(b);
			alpha[by][bx] = static_cast<float>(a);

			rgb_content |= r != 0 || g != 0 || b != 0;
			a_content |= a != 0;
		}
	}

	// if there is alpha but no RGB content, replace RGB content with alpha content
	// (GUID_WICPixelFormat32bppPBGRA mode seem to zero RGB content if it can
	// replace it with alpha content alone.)
	if (a_content && !rgb_content) {
		for (auto y = 0; y < n_intensity_block_divisions; y++) {
			for (auto x = 0; x < n_intensity_block_divisions; x++) {
				intensities[y][x].r = alpha[y][x];
				intensities[y][x].g = alpha[y][x];
				intensities[y][x].b = alpha[y][x];
			}
		}
	}

	// normalize

	auto intensity_min = std::numeric_limits<float>::max();
	auto intensity_max = 0.0f;
	for (auto y = 0; y < n_intensity_block_divisions; y++) {
		for (auto x = 0; x < n_intensity_block_divisions; x++) {
			intensity_min = std::min(intensity_min, intensities[y][x].r);
			intensity_min = std::min(intensity_min, intensities[y][x].g);
			intensity_min = std::min(intensity_min, intensities[y][x].b);
			intensity_max = std::max(intensity_max, intensities[y][x].r);
			intensity_max = std::max(intensity_max, intensities[y][x].g);
			intensity_max = std::max(intensity_max, intensities[y][x].b);
		}
	}

	if (intensity_max - intensity_min != 0) {
		for (auto y = 0; y < n_intensity_block_divisions; y++) {
			for (auto x = 0; x < n_intensity_block_divisions; x++) {
				intensities[y][x].r = (intensities[y][x].r - intensity_min) / (intensity_max - intensity_min);
				intensities[y][x].g = (intensities[y][x].g - intensity_min) / (intensity_max - intensity_min);
				intensities[y][x].b = (intensities[y][x].b - intensity_min) / (intensity_max - intensity_min);
			}
		}
	}

	return intensities;
}

void Image::load_pixels(IWICBitmapFrameDecode* const frame) {
	er = frame->GetSize(&image_size.w, &image_size.h);
	assert(image_size.w > 0 && image_size.h > 0);

	ComPtr<IWICImagingFactory> wic_factory;
	er = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&wic_factory));

	ComPtr<IWICFormatConverter> format_converter;
	er = wic_factory->CreateFormatConverter(&format_converter);
	er = format_converter->Initialize(
		frame,
		GUID_WICPixelFormat32bppPBGRA,
		WICBitmapDitherTypeNone,
		nullptr,
		0,
		WICBitmapPaletteTypeCustom);

	const auto pixel_stride = 4;
	const auto line_stride = image_size.w * pixel_stride;
	const std::size_t pixel_buffer_size = line_stride * image_size.h;
	assert(pixel_buffer_size > 0);
	std::vector<uint8_t> pixel_buffer(pixel_buffer_size);

	auto hr = format_converter->CopyPixels(
		nullptr,
		line_stride,
		numeric_cast<UINT>(pixel_buffer_size),
		pixel_buffer.data());
	if (FAILED(hr)) {
		status = Status::decode_failed;
		return;
	}

	intensities = calculate_intensities(pixel_buffer, pixel_stride, line_stride,
		D2D1::RectU(0, 0, image_size.w, image_size.h));

	auto square_size = std::min(image_size.w, image_size.h);
	intensities_cropped_1 = calculate_intensities(pixel_buffer, pixel_stride, line_stride,
		D2D1::RectU(0, 0, square_size, square_size));
	intensities_cropped_2 = calculate_intensities(pixel_buffer, pixel_stride, line_stride,
		D2D1::RectU(image_size.w - square_size, image_size.h - square_size, image_size.w, image_size.h));
}

static std::wstring widen(const std::string& string) {
	std::wostringstream os;
	auto& f = std::use_facet<std::ctype<wchar_t>>(os.getloc());    
	for (const auto c : string)
		os << f.widen(c);
	return os.str();
}

std::wstring get_propvariant_string(const PROPVARIANT& pv) {
	if (pv.vt == VT_LPSTR)
		return trim(widen(pv.pszVal));
	else if (pv.vt == VT_LPWSTR)
		return trim(pv.pwszVal);
	else
		return L"";
}

std::chrono::system_clock::time_point get_propvariant_time(const PROPVARIANT& pv) {
	std::wstring s = get_propvariant_string(pv);

	bool valid = false;
	if (s.length() >= std::wstring{L"0000-00-00"}.length()) {
		valid = true;

		valid &=
			(s[4] == ':' || s[4] == '-') &&
			(s[7] == ':' || s[7] == '-');

		if (s.length() >= std::wstring{L"0000-00-00 00:00"}.length())
			valid &=
				(s[10] == 'T' || s[10] == ' ') &&
				(s[13] == ':');

		if (s.length() >= std::wstring{L"0000-00-00 00:00:00"}.length())
			valid &=
				(s[16] == ':' || s[16] == '+' || s[16] == '-');
	}

	if (!valid)
		return std::chrono::system_clock::time_point::min();

	std::size_t pos;

	pos = s.find_last_of(L'+');
	if (pos != std::string::npos && pos > 15)
		s.erase(pos);

	pos = s.find_last_of(L'-');
	if (pos != std::string::npos && pos > 15)
		s.erase(pos);

	tm tm{0};
	std::wistringstream ss{s};

	ss >> tm.tm_year;
	tm.tm_year -= 1900;
	ss.ignore(1);
	ss >> tm.tm_mon;
	tm.tm_mon -= 1;
	ss.ignore(1);
	ss >> tm.tm_mday;

	ss.ignore(1);
	ss >> tm.tm_hour;
	ss.ignore(1);
	ss >> tm.tm_min;
	ss.ignore(1);
	ss >> tm.tm_sec;

	tm.tm_isdst = -1;

	auto t = mktime(&tm);
	if (t == -1)
		return std::chrono::system_clock::time_point::min();
	return std::chrono::system_clock::from_time_t(t);
}

float get_propvariant_location(const PROPVARIANT& pv) {
	auto count = PropVariantGetElementCount(pv);
	assert(count == 3);
	if (count != 3)
		return 0;

	ULONGLONG data[3];
	er = PropVariantGetUInt64Elem(pv, 0, &data[0]);
	er = PropVariantGetUInt64Elem(pv, 1, &data[1]);
	er = PropVariantGetUInt64Elem(pv, 2, &data[2]);

	auto ul_data = reinterpret_cast<ULONG*>(data);
	auto d = static_cast<float>(ul_data[0]) / ul_data[1];
	auto m = static_cast<float>(ul_data[2]) / ul_data[3];
	auto s = static_cast<float>(ul_data[4]) / ul_data[5];

	return d + m/60 + s/3600;
}

void Image::load_metadata(IWICBitmapFrameDecode* const frame) {
	HRESULT hr;

	ComPtr<IWICMetadataQueryReader> reader;
	hr = frame->GetMetadataQueryReader(&reader);
	if (SUCCEEDED(hr)) {
		// xmp dates: YYYY YYYY-MM YYYY-MM-DD YYYY-MM-DDThh:mmTZD YYYY-MM-DDThh:mm:ssTZD YYYY-MM-DDThh:mm:ss.sTZD
		// tiff/exif dates (digits may be "blank"): YYYY:MM:DD HH:MM:SS
		const wchar_t* const date_tags[] {
			// tiff
			L"/ifd/{ushort=306}", // DateTime
			L"/ifd/exif/{ushort=36867}", // DateTimeOriginal
			L"/ifd/exif/{ushort=36868}", // DateTimeDigitized

			// jpeg
			L"/app1/ifd/{ushort=306}", // DateTime
			L"/app1/ifd/exif/{ushort=36867}", // DateTimeOriginal
			L"/app1/ifd/exif/{ushort=36868}", // DateTimeDigitized

			L"/xmp/exif:DateTimeDigitized",
			L"/xmp/exif:DateTimeOriginal",
			L"/xmp/exif:GPSTimeStamp",
			L"/xmp/xmp:CreateDate",
			L"/xmp/xmp:MetadataDate",
			L"/xmp/xmp:ModifyDate",
			L"/xmp/photoshop:DateCreated",

			#if 0
			L"/app13/irb/8bimiptc/iptc/Date Created",
			L"/app13/irb/8bimiptc/iptc/Time Created",
			L"/app13/irb/8bimiptc/iptc/Digital Creation Date",
			L"/app13/irb/8bimiptc/iptc/Digital Creation Time",
			L"/xmp/acdsee:Datetime",
			L"/xmp/dc:Date",
			L"/xmp/tiff:DateTime",
			L"/ifd/iptc/{str=DateCreated}",
			L"/ifd/iptc/Date Created",
			L"/app13/irb/8bimiptc/iptc/{str=DateCreated}",
			L"/app4/fpxr/DataCreateDate",
			L"/app4/fpxr/DataModifyDate",
			#endif
		};

		PROPVARIANT value;
		PropVariantInit(&value);

		// metadata times

		for (const auto& tag : date_tags) {
			hr = reader->GetMetadataByName(tag, &value);
			if (SUCCEEDED(hr)) {
				auto t = get_propvariant_time(value);
				if (t > std::chrono::system_clock::time_point::min())
					metadata_times.push_back(t);
			}
			er = PropVariantClear(&value);
		}
		sort(metadata_times.begin(), metadata_times.end());
		auto new_end = std::unique(metadata_times.begin(), metadata_times.end());
		metadata_times.erase(new_end, metadata_times.end());

		// metadata make and model

		hr = reader->GetMetadataByName(L"/app1/ifd/{ushort=271}", &value);
		if (SUCCEEDED(hr))
			metadata_make_model += get_propvariant_string(value);
		er = PropVariantClear(&value);
		hr = reader->GetMetadataByName(L"/app1/ifd/{ushort=272}", &value);
		if (SUCCEEDED(hr))
			metadata_make_model += L" " + get_propvariant_string(value);
		er = PropVariantClear(&value);
		hr = reader->GetMetadataByName(L"/app1/ifd/exif/{ushort=42033}", &value);

		if (!metadata_make_model.empty()) {
			// replace some common phrases in make/model
			struct {
				std::wstring substring;
				std::wstring substring_replacement;
			} metadata_make_model_replacements[] {
				{L"NIKON CORPORATION", L"NIKON"} ,
				{L"EASTMAN KODAK COMPANY", L"KODAK"},
				{L" ZOOM DIGITAL CAMERA", L""},
			};
			for (const auto& r : metadata_make_model_replacements) {
				auto fp = metadata_make_model.find(r.substring);
				if (fp != std::wstring::npos)
					metadata_make_model.replace(fp, r.substring.length(), r.substring_replacement);
			}

			// remove identical consecutive words
			std::wistringstream ss{metadata_make_model};
			std::vector<std::wstring> words;
			std::wstring word;
			while (ss >> word)
				words.push_back(word);
			words.erase(std::unique(words.begin(), words.end()), words.end());
			metadata_make_model = L"";
			for (const auto& w : words) {
				if (!metadata_make_model.empty())
					metadata_make_model.append(L" ");
				metadata_make_model.append(w);
			}
		}

		// metadata camera id

		if (SUCCEEDED(hr))
			metadata_camera_id += get_propvariant_string(value);
		er = PropVariantClear(&value);

		// metadata image id

		hr = reader->GetMetadataByName(L"/app1/ifd/exif/{ushort=42016}", &value);
		if (SUCCEEDED(hr))
			metadata_image_id = get_propvariant_string(value);
		er = PropVariantClear(&value);

		// metadata position

		hr = reader->GetMetadataByName(L"/app1/ifd/gps/{ushort=2}", &value);
		if (SUCCEEDED(hr)) {
			metadata_position.y = get_propvariant_location(value);
			er = PropVariantClear(&value);

			hr = reader->GetMetadataByName(L"/app1/ifd/gps/{ushort=1}", &value);
			if (SUCCEEDED(hr)) {
				std::wstring s = get_propvariant_string(value);
				if (s == L"S" || s == L"s")
					metadata_position.y *= -1;
				else if (s != L"N" && s != L"n")
					metadata_position.y = 0;
			} else {
				metadata_position.y = 0;
			}
		}
		er = PropVariantClear(&value);

		hr = reader->GetMetadataByName(L"/app1/ifd/gps/{ushort=4}", &value);
		if (SUCCEEDED(hr)) {
			metadata_position.x = get_propvariant_location(value);
			er = PropVariantClear(&value);

			hr = reader->GetMetadataByName(L"/app1/ifd/gps/{ushort=3}", &value);
			if (SUCCEEDED(hr)) {
				std::wstring s = get_propvariant_string(value);
				if (s == L"W" || s == L"w")
					metadata_position.x *= -1;
				else if (s != L"E" && s != L"e")
					metadata_position.x = 0;
			} else {
				metadata_position.x = 0;
			}
		}
		er = PropVariantClear(&value);

		if (metadata_position.x == 0 || metadata_position.y == 0)
			metadata_position = {0, 0}; // TODO: bad way to indicate invalid position
	}
}

void Image::calculate_hash() const {
	std::vector<std::uint8_t> buffer(numeric_cast<std::size_t>(file_size()));
	auto frame = get_frame(buffer);
	if (frame == nullptr)
		return;

	file_hash = Hash(buffer.data(), buffer.size());

	ComPtr<IWICImagingFactory> wic_factory;
	er = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&wic_factory));

	ComPtr<IWICBitmap> bitmap;
	er = wic_factory->CreateBitmapFromSource(frame, WICBitmapNoCache, &bitmap);
	ComPtr<IWICBitmapLock> bitmap_lock;
	auto hr = bitmap->Lock(nullptr, WICBitmapLockRead, &bitmap_lock);
	if (SUCCEEDED(hr)) {
		UINT size;
		std::uint8_t* pixel_data;
		er = bitmap_lock->GetDataPointer(&size, static_cast<BYTE**>(&pixel_data));
		pixel_hash = Hash(pixel_data, size);
	}
}

ComPtr<IWICBitmapFrameDecode> Image::get_frame(std::vector<std::uint8_t>& buffer) const {
	std::ifstream ifs{path_, std::ios::binary};
	ifs.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
	assert(numeric_cast<std::size_t>(ifs.gcount()) == buffer.size() || (ifs.fail() && ifs.gcount() == 0));
	if (ifs.fail())
		return nullptr;
	ifs.close();

	ComPtr<IWICImagingFactory> wic_factory;
	er = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&wic_factory));

	ComPtr<IWICStream> stream;
	er = wic_factory->CreateStream(&stream);
	auto hr = stream->InitializeFromMemory(buffer.data(), numeric_cast<DWORD>(buffer.size()));
	if (FAILED(hr))
		return nullptr;

	ComPtr<IWICBitmapDecoder> decoder;
	hr = wic_factory->CreateDecoderFromStream(
		stream,
		nullptr,
		WICDecodeMetadataCacheOnDemand,
		&decoder);
	if (FAILED(hr))
		return nullptr;

	ComPtr<IWICBitmapFrameDecode> frame;
	er = decoder->GetFrame(0, &frame);

	if (image_size != Size2u{0, 0}) {
		Size2u s;
		er = frame->GetSize(&s.w, &s.h);
		if (s != image_size)
			return nullptr;
	}

	return frame;
}

ComPtr<ID2D1Bitmap> Image::get_bitmap(ID2D1HwndRenderTarget* const render_target) const {
	const auto bitmap_cache_limit = 8;

	BitmapCacheEntry bce;
	{
		std::lock_guard<std::mutex> lg{bitmap_cache_mutex};
		auto i = std::find_if(bitmap_cache.begin(), bitmap_cache.end(),
			[&](BitmapCacheEntry bce) {
				return bce.image.lock() == shared_from_this();
			});
		if (i != bitmap_cache.end()) {
			bce = *i;
			bitmap_cache.erase(i);
		}
	}

	if (bce.bitmap == nullptr) {
		bce.image = shared_from_this();

		std::vector<std::uint8_t> data(numeric_cast<std::size_t>(file_size()));
		auto frame = get_frame(data);
		if (frame == nullptr)
			return nullptr;

		ComPtr<IWICImagingFactory> wic_factory;
		er = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&wic_factory));

		// corresponding wic and direct2d pixel formats
		const auto wic_pf = GUID_WICPixelFormat32bppPBGRA;
		const auto d2d_pf = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);

		ComPtr<IWICFormatConverter> format_converter;
		er = wic_factory->CreateFormatConverter(&format_converter);
		er = format_converter->Initialize(
			frame, wic_pf,	WICBitmapDitherTypeNone,
			nullptr, 0, WICBitmapPaletteTypeCustom);

		Point2f dpi;
		render_target->GetDpi(&dpi.x, &dpi.y);
		er = render_target->CreateBitmapFromWicBitmap(
			format_converter, D2D1::BitmapProperties(d2d_pf, dpi.x, dpi.y), &bce.bitmap);
	}

	{
		std::lock_guard<std::mutex> lg{bitmap_cache_mutex};
		bitmap_cache.push_back(bce);
		if (bitmap_cache.size() > bitmap_cache_limit)
			bitmap_cache.erase(bitmap_cache.begin());
	}

	return bce.bitmap;
}
