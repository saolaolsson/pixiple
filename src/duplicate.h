#pragma once

#include "image.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

class Duplicate {
public:
	std::shared_ptr<Image> image_1;
	std::shared_ptr<Image> image_2;
	float distance;

	Duplicate(
		const std::shared_ptr<Image>& image_1,
		const std::shared_ptr<Image>& image_2,
		const float distance)
		:
		image_1{image_1},
		image_2{image_2},
		distance{distance}
	{
		assert(distance >= 0);
		if (this->image_2->get_file_time() < this->image_1->get_file_time())
			std::swap(this->image_1, this->image_2);
	}

	bool is_in_same_folder() const {
		auto name_start_1 = image_1->get_path().find_last_of(L'\\');
		auto name_start_2 = image_2->get_path().find_last_of(L'\\');

		assert(name_start_1 != std::string::npos && name_start_2 != std::string::npos);

		return
			image_1->get_path().substr(0, name_start_1) ==
			image_2->get_path().substr(0, name_start_2);
	}

	std::chrono::system_clock::duration get_age() const {
		auto now = std::chrono::system_clock::now();
		return std::min(now - image_1->get_file_time(), now - image_2->get_file_time());
	}

	bool operator<(const Duplicate& rhs) const {
		if (distance != rhs.distance) {
			return distance < rhs.distance;
		} else {
			// same distance, so sort by file names instead
			auto& i = image_1->get_path() < image_2->get_path() ? *image_1 : *image_2;
			const auto& rhsi = rhs.image_1->get_path() < rhs.image_2->get_path() ? *rhs.image_1 : *rhs.image_2;
			return i.get_path() < rhsi.get_path();
		}
	}
};
