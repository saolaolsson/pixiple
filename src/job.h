#pragma once

#include "image_pair.h"

#include <atomic>
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

	Job(const std::vector<std::filesystem::path>& paths,
		std::vector<ImagePair>& pairs_visual,
		std::vector<ImagePair>& pairs_time,
		std::vector<ImagePair>& pairs_location,
		std::vector<ImagePair>& pairs_combined)
		:
		paths{paths},
		pairs_visual{pairs_visual},
		pairs_time{pairs_time},
		pairs_location{pairs_location},
		pairs_combined{pairs_combined}
	{
	}

	ImagePair get_next_pair();
	float get_progress() const;
	bool is_completed() const;

private:
	std::size_t progress_current() const;
	std::size_t progress_total() const;

	const std::vector<std::filesystem::path>& paths;

	mutable std::mutex mutex;
	std::vector<std::shared_ptr<Image>> images{paths.size()};
	std::size_t index_minor = 0;
	std::size_t index_major = 0;
	std::size_t index_next_to_create = 0;
};
