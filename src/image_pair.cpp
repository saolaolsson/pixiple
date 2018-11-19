#include "shared.h"

#include "image_pair.h"

#include "time.h"

#include <iomanip>
#include <sstream>

ImagePair::ImagePair(
	const std::shared_ptr<Image>& image_1,
	const std::shared_ptr<Image>& image_2)
	:
	image_1{image_1},
	image_2{image_2}
{
	if (image_1 && image_2)
		if (this->image_2->file_time() < this->image_1->file_time())
			std::swap(this->image_1, this->image_2);
}

bool ImagePair::operator<(const ImagePair& rhs) const {
	if (distance != rhs.distance) {
		return distance < rhs.distance;
	} else {
		// same distance, so sort by file names instead
		auto& i = image_1->path() < image_2->path() ? *image_1 : *image_2;
		const auto& rhsi = rhs.image_1->path() < rhs.image_2->path() ? *rhs.image_1 : *rhs.image_2;
		return i.path() < rhsi.path();
	}
}

bool ImagePair::is_in_same_folder() const {
	return image_1->path().parent_path() == image_2->path().parent_path();
}

std::chrono::system_clock::duration ImagePair::get_age() const {
	auto now = std::chrono::system_clock::now();
	return std::min(now - image_1->file_time(), now - image_2->file_time());
}

std::chrono::system_clock::duration ImagePair::time_distance() const {
	auto duration_min = std::chrono::system_clock::duration::max();
	for (auto t1 : image_1->get_metadata_times())
		for (auto t2 : image_2->get_metadata_times())
			duration_min = std::min(duration_min, std::chrono::abs(t1 - t2));
	return duration_min;
}

float earth_distance(const Point2f& p1, const Point2f& p2) {
	assert(p1.x >= -180 && p1.x <= 180);
	assert(p2.x >= -180 && p2.x <= 180);
	assert(p1.y >= -90 && p1.y <= 90);
	assert(p2.y >= -90 && p2.y <= 90);

	const auto earth_mean_radius = 6371*1000.0f;
	const auto pi = 3.14159265358979323846f;

	auto p1r = Point2f{p1.x * (pi / 180), p1.y * (pi / 180)};
	auto p2r = Point2f{p2.x * (pi / 180), p2.y * (pi / 180)};

	auto dy = p2r.y - p1r.y;
	auto dx = p2r.x - p1r.x;

	auto a =
		sin(dy / 2) * sin(dy / 2) +
		cos(p1r.y) * cos(p2r.y) *
		sin(dx / 2) * sin(dx / 2);
	auto c = 2 * atan2(sqrt(a), sqrt(1-a));
	auto d = earth_mean_radius * c;

	assert(d >= 0);
	assert(d <= earth_mean_radius * earth_mean_radius * pi);

	return d;
}

float ImagePair::location_distance() const {
	auto p1 = image_1->get_metadata_position();
	auto p2 = image_2->get_metadata_position();
	if (p1.x != 0 && p1.y != 0 && p2.x != 0 && p2.y != 0)
		return earth_distance(p1, p2);
	else
		return std::numeric_limits<float>::max();
}

std::wstring ImagePair::description() const {
	std::wostringstream ss;
	ss << L"Distance " << std::setprecision(3) << distance;

	if (auto td = time_distance(); td != std::chrono::system_clock::duration::max())
		ss << L", " << td;

	if (auto ld = location_distance(); ld != std::numeric_limits<float>::max()) {
		if (ld > 3*1000)
			ss << L", " << static_cast<int>(ld / 1000 + 0.5f) << " kilometers";
		else
			ss << L", " << static_cast<int>(ld + 0.5f) << " meter" << (ld <= 0.5f || ld >= 1.5f ? L"s" : L"");
	}

	return ss.str();
}
