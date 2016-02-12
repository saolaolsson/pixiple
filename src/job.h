#pragma once

#include "duplicate.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Image;

class Job {
public:
	std::vector<Duplicate>& duplicates_visual;
	std::vector<Duplicate>& duplicates_time;
	std::vector<Duplicate>& duplicates_location;
	std::vector<Duplicate>& duplicates_combined;
	std::mutex duplicates_mutex;

	std::atomic<bool> force_thread_exit = false;

	Job(const std::vector<std::wstring>& paths,
		std::vector<Duplicate>& duplicates_visual,
		std::vector<Duplicate>& duplicates_time,
		std::vector<Duplicate>& duplicates_location,
		std::vector<Duplicate>& duplicates_combined)
		:
		paths{paths},
		duplicates_visual{duplicates_visual},
		duplicates_time{duplicates_time},
		duplicates_location{duplicates_location},
		duplicates_combined{duplicates_combined}
	{
	}

	std::pair<std::shared_ptr<Image>, std::shared_ptr<Image>> get_next_pair();
	float get_progress() const;
	bool is_completed() const;

private:
	const std::vector<std::wstring>& paths;
	std::vector<std::shared_ptr<Image>> images{paths.size()};

	std::size_t index_minor = 0;
	std::size_t index_major = 0;
	std::size_t index_next_to_create = 0;
	std::mutex index_mutex;

	std::atomic<float> progress = 0;
};
