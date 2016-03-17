#include "shared.h"

#include "job.h"

#include "image.h"

ImagePair Job::get_next_pair() {
	std::unique_lock<std::mutex> ul{index_mutex};

	if (is_completed())
		return {nullptr, nullptr};

	while (images[index_major] == nullptr) {
		if (index_next_to_create < images.size()) {
			// create image
			auto i = index_next_to_create++;
			ul.unlock();
			auto image = std::make_shared<Image>(paths[i]);
			ul.lock();
			images[i] = image;
		} else {
			// no more images to create but allow other threads to finish creating images
			ul.unlock(); 
			ul.lock();
		}
	}

	auto index_minor_old = index_minor;
	auto index_major_old = index_major;

	if (index_minor == index_major) {
		if (index_major + 1 == images.size()) {
			progress = 1.0f;
			return {nullptr, nullptr};
		}
		index_major++;
		index_minor = 0;
	} else {
		index_minor++;
	}

	if (index_major == images.size())
		progress = 1.0f;
	else
		progress = static_cast<float>(index_major) / images.size();

	return {images[index_minor_old], images[index_major_old]};
}

float Job::get_progress() const {
	return progress;
}

bool Job::is_completed() const {
	return progress == 1.0f;
}
