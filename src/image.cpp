#include "shared.h"

#include "image.h"

#include "shared/numeric_cast.h"
#include "shared/trim.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <propvarutil.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <wincodec.h>

std::mutex Image::class_mutex;
int Image::n_instances;
std::vector<Image::BitmapCacheEntry> Image::bitmap_cache;

std::vector<std::vector<float>> convolve(
	const std::vector<std::vector<float>>& input,
	const std::vector<std::vector<float>>& kernel
) {
	const Size2i s{numeric_cast<int>(input[0].size()), numeric_cast<int>(input.size())};
	const Size2i sk{numeric_cast<int>(kernel[0].size()), numeric_cast<int>(kernel.size())};

	std::vector<std::vector<float>> output = input;
	for (int y = 0; y < s.h; y++) {
		for (int x = 0; x < s.w; x++) {
			output[y][x] = 0;
			for (int yk = 0; yk < sk.h; yk++) {
				for (int xk = 0; xk < sk.w; xk++) {
					if (y+yk >= s.h || x+xk >= s.w)
						continue;
					output[y][x] += input[y+yk][x+xk];
				}
			}
		}
	}
	return output;
}

float variance(const std::vector<std::vector<float>>& input) {
	const Size2i s{numeric_cast<int>(input[0].size()), numeric_cast<int>(input.size())};

	float sum = 0;
	for (int y = 0; y < s.h; y++)
		for (int x = 0; x < s.w; x++)
			sum += input[y][x];
	auto average = sum / (s.h*s.w);

	float variance = 0;
	for (int y = 0; y < s.h; y++)
		for (int x = 0; x < s.w; x++)
			variance += (input[y][x] - average)*(input[y][x] - average);
	variance /= (s.h*s.w);

	return variance;
}

void Image::clear_cache() {
	bitmap_cache.clear();
}

Image::Image(const std::tr2::sys::path& path) : path_{path} {
	assert(!path.empty());

	file_size_ = std::tr2::sys::file_size(path);
	file_time_ = std::tr2::sys::last_write_time(path);

	std::vector<std::uint8_t> data(numeric_cast<std::size_t>(file_size_));
	auto frame = get_frame(data);
	if (frame) {
		load_pixels(frame);
		load_metadata(frame);
	} else {
		status = Status::open_failed;
	}

	std::lock_guard<std::mutex> lg{class_mutex};
	n_instances++;
}

Image::~Image() {
	std::lock_guard<std::mutex> lg{class_mutex};
	n_instances--;
	if (n_instances == 0)
		clear_cache();
}

Image::Status Image::get_status() const {
	return status;
}

std::tr2::sys::path Image::path() const {
	return path_;
}

std::uintmax_t Image::file_size() const {
	return file_size_;
}

std::chrono::system_clock::time_point Image::file_time() const {
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
		const_cast<Image*>(this)->calculate_hash();
	return file_hash;
}

Hash Image::get_pixel_hash() const {
	if (pixel_hash == Hash{})
		const_cast<Image*>(this)->calculate_hash();
	return pixel_hash;
}

float Image::get_blur() const {
	if (blur == 0.0f)
		const_cast<Image*>(this)->calculate_blur();
	return blur;
}

float Image::get_distance(const Image& image, const float maximum_distance) const {
	if (status != Status::ok || image.status != Status::ok)
		return std::numeric_limits<float>::max();

	Transform transforms[] {
		Transform::none,
		Transform::rotate_90,
		Transform::rotate_180,
		Transform::rotate_270,
		Transform::flip_h,
		Transform::flip_v,
		Transform::flip_nw_se,
		Transform::flip_sw_ne,
	};

	auto sum = std::numeric_limits<float>::max();
	for (auto t : transforms) {
		auto s = 0.0f;
		for (int y = 0; y < n_intensity_block_divisions; y++) {
			for (int x = 0; x < n_intensity_block_divisions; x++) {
				Colour c1 = get_intensity(x, y);
				Colour c2 = image.get_intensity(x, y, t);
				s += (c2.r - c1.r)*(c2.r - c1.r) + (c2.g - c1.g)*(c2.g - c1.g) + (c2.b - c1.b)*(c2.b - c1.b);
			}
			if (s > sum || s > maximum_distance * maximum_distance * n_intensity_block_divisions * n_intensity_block_divisions)
				break; // optimization only
		}
		sum = std::min(sum, s);
	}
	assert(sum == sum);
	return sqrt(sum / n_intensity_block_divisions / n_intensity_block_divisions);
}

void Image::draw(
	ID2D1HwndRenderTarget* const render_target,
	const D2D1_RECT_F& rect_dest,
	const D2D1_RECT_F& rect_src,
	const D2D1_BITMAP_INTERPOLATION_MODE& interpolation_mode
) const {
	auto bitmap = get_bitmap(render_target);
	if (bitmap) {
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
}

static std::wstring to_windows_path(const std::tr2::sys::path& path) {
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
	er = fo->PerformOperations();
}

// Try to open explorer window at containing folder with file
// selected, or try to open containing folder, or fail silently.
void Image::open_folder() const {
	__unaligned auto folder = ILCreateFromPath(path_.parent_path().c_str());
	if (folder == nullptr)
		return;

	__unaligned auto file = ILCreateFromPath(path_.c_str());
	if (file) {
		__unaligned const ITEMIDLIST* selection[]{file};
		er = SHOpenFolderAndSelectItems(folder, 1, selection, 0);
		CoTaskMemFree(file);
	}
	else {
		__unaligned const ITEMIDLIST* selection[]{folder};
		er = SHOpenFolderAndSelectItems(folder, 1, selection, 0);
	}

	CoTaskMemFree(folder);
}

void Image::load_pixels(ComPtr<IWICBitmapFrameDecode> frame) {
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

	// fill intensities

	bool rgb_content = false;
	bool a_content = false;
	float alpha[n_intensity_block_divisions][n_intensity_block_divisions];

	for (auto by = 0; by < n_intensity_block_divisions; by++) {
		for (auto bx = 0; bx < n_intensity_block_divisions; bx++) {
			const auto offset_x = image_size.w * bx / n_intensity_block_divisions;
			const auto offset_y = image_size.h * by / n_intensity_block_divisions;
			const auto offset_x_next = image_size.w * (bx + 1) / n_intensity_block_divisions;
			const auto offset_y_next = image_size.h * (by + 1) / n_intensity_block_divisions;

			std::uint32_t r = 0;
			std::uint32_t g = 0;
			std::uint32_t b = 0;
			std::uint32_t a = 0;

			for (auto y = offset_y; y < offset_y_next; y++) {
				for (auto x = offset_x; x < offset_x_next; x++) {
					b += pixel_buffer.data()[y*line_stride + x*pixel_stride + 0];
					g += pixel_buffer.data()[y*line_stride + x*pixel_stride + 1];
					r += pixel_buffer.data()[y*line_stride + x*pixel_stride + 2];
					a += pixel_buffer.data()[y*line_stride + x*pixel_stride + 3];
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

void Image::load_metadata(ComPtr<IWICBitmapFrameDecode> frame) {
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

void Image::calculate_hash() {
	std::vector<std::uint8_t> data(numeric_cast<std::size_t>(file_size_));
	auto frame = get_frame(data);
	if (frame == nullptr)
		return;

	file_hash = Hash(data.data(), data.size());

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

void Image::calculate_blur() {
	std::vector<std::uint8_t> data(numeric_cast<std::size_t>(file_size_));
	auto frame = get_frame(data);
	if (frame == nullptr)
		return;

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
	if (FAILED(hr))
		return;

	std::vector<std::vector<float>> intensities(image_size.h, std::vector<float>(image_size.w));
	for (unsigned y = 0; y < image_size.h; y++)
		for (unsigned x = 0; x < image_size.w; x++)
			intensities[y][x] = (
				pixel_buffer.data()[y*line_stride + x*pixel_stride + 0] +
				pixel_buffer.data()[y*line_stride + x*pixel_stride + 1] +
				pixel_buffer.data()[y*line_stride + x*pixel_stride + 2]) / 255.0f / 3.0f;

	std::vector<std::vector<float>> kernel{
		{0,  1, 0},
		{1, -4, 1},
		{0,  1, 0},
	};
	blur = variance(convolve(intensities, kernel));
}

ComPtr<IWICBitmapFrameDecode> Image::get_frame(std::vector<std::uint8_t>& buffer) const {
	std::ifstream ifs(path_, std::ios::binary);
	ifs.read(reinterpret_cast<char*>(buffer.data()), file_size_);
	assert(numeric_cast<std::size_t>(ifs.gcount()) == file_size_ || (ifs.fail() && ifs.gcount() == 0));
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

	er = stream->InitializeFromMemory(
		buffer.data(),
		numeric_cast<DWORD>(file_size_));

	ComPtr<IWICBitmapDecoder> decoder;
	auto hr = wic_factory->CreateDecoderFromStream(
		stream,
		nullptr,
		WICDecodeMetadataCacheOnDemand,
		&decoder);
	if (FAILED(hr))
		return nullptr;

	ComPtr<IWICBitmapFrameDecode> frame;
	er = decoder->GetFrame(0, &frame);

	// if file has changed since *this was created, fail

	if (file_size_ != 0)
		if (std::tr2::sys::file_size(path_) != file_size_)
			return nullptr;

	if (image_size.w != 0 || image_size.h != 0) {
		std::uint32_t w;
		std::uint32_t h;
		er = frame->GetSize(&w, &h);
		if (w != image_size.w || h != image_size.h)
			return nullptr;
	}

	return frame;
}

ComPtr<ID2D1Bitmap> Image::get_bitmap(ID2D1HwndRenderTarget* const render_target) const {
	const auto bitmap_cache_limit = 8;

	BitmapCacheEntry bce;
	{
		std::lock_guard<std::mutex> lg{class_mutex};
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

		std::vector<std::uint8_t> data(numeric_cast<std::size_t>(file_size_));
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
		std::lock_guard<std::mutex> lg{class_mutex};
		bitmap_cache.push_back(bce);
		if (bitmap_cache.size() > bitmap_cache_limit)
			bitmap_cache.erase(bitmap_cache.begin());
	}

	return bce.bitmap;
}

Image::Colour Image::get_intensity(const int x, const int y, const Transform transform) const {
	assert(x >= 0 && x < n_intensity_block_divisions);
	assert(y >= 0 && y < n_intensity_block_divisions);

	auto xt = x;
	auto yt = y;

	switch (transform) {
	case Transform::none:
		break;
	case Transform::rotate_90:
		xt = n_intensity_block_divisions - 1 - y;
		yt = x;
		break;
	case Transform::rotate_180:
		xt = n_intensity_block_divisions - 1 - x;
		yt = n_intensity_block_divisions - 1 - y;
		break;
	case Transform::rotate_270:
		xt = y;
		yt = n_intensity_block_divisions - 1 - x;
		break;
	case Transform::flip_h:
		xt = n_intensity_block_divisions - 1 - x;
		yt = y;
		break;
	case Transform::flip_v:
		xt = x;
		yt = n_intensity_block_divisions - 1 - y;
		break;
	case Transform::flip_nw_se:
		xt = y;
		yt = x;
		break;
	case Transform::flip_sw_ne:
		xt = n_intensity_block_divisions - 1 - y;
		yt = n_intensity_block_divisions - 1 - x;
		break;
	default:
		assert(false);
	}

	return intensities[yt][xt];
}
