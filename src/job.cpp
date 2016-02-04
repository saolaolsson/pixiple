#include "shared.h"

#include "job.h"

#include "image.h"

std::pair<std::shared_ptr<Image>, std::shared_ptr<Image>> Job::get_next_pair() {
	std::unique_lock<std::mutex> ul{index_mutex};

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

	auto i_minor = index_minor;
	auto i_major = index_major;

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

	progress = static_cast<float>(index_major) / images.size();
	return {images[i_minor], images[i_major]};
}

float Job::get_progress() const {
	return progress;
}

bool Job::is_completed() const {
	return progress == 1.0f;
}
