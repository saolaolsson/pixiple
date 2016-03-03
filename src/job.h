#pragma once

#include "image_pair.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Image;

class Job {
public:
	std::vector<ImagePair>& pairs_visual;
	std::vector<ImagePair>& pairs_time;
	std::vector<ImagePair>& pairs_location;
	std::vector<ImagePair>& pairs_combined;
	std::mutex pairs_mutex;

	std::atomic<bool> force_thread_exit = false;

	Job(const std::vector<std::tr2::sys::path>& paths,
		std::vector<ImagePair>& pairs_visual,
		std::vector<ImagePair>& pairs_time,
		std::vector<ImagePair>& pairs_location,
		std::vector<ImagePair>& pairs_combined)
		:
		paths{paths},
		pairs_visual{pairs_visual},
		pairs_time{pairs_time},
		pairs_location{pairs_location},
		pairs_combined{pairs_combined},
		progress{paths.empty() ? 1.0f : 0.0f}
	{
	}

	ImagePair get_next_pair();
	float get_progress() const;
	bool is_completed() const;

private:
	const std::vector<std::tr2::sys::path>& paths;
	std::vector<std::shared_ptr<Image>> images{paths.size()};

	std::size_t index_minor = 0;
	std::size_t index_major = 0;
	std::size_t index_next_to_create = 0;
	std::mutex index_mutex;

	std::atomic<float> progress = 0;
};
