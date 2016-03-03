#pragma once

#include "image.h"

#include <chrono>
#include <memory>
#include <string>

class ImagePair {
public:
	std::shared_ptr<Image> image_1;
	std::shared_ptr<Image> image_2;
	float distance;

	ImagePair(
		const std::shared_ptr<Image>& image_1,
		const std::shared_ptr<Image>& image_2,
		const float distance = 0);

	bool operator<(const ImagePair& rhs) const;

	bool is_in_same_folder() const;
	std::chrono::system_clock::duration get_age() const;
	std::chrono::system_clock::duration time_distance() const;
	float location_distance() const;
	std::wstring description() const;
};
