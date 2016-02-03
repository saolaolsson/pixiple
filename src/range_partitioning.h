#pragma once

#include "range.h"

#include <memory>
#include <mutex>
#include <vector>

class RangePartitioning {
public:
	RangePartitioning(std::size_t size);

	std::unique_ptr<Range> get_next();

	std::size_t get_size() const;
	std::size_t get_progress() const;

private:
	static const std::size_t range_size = 32;

	std::size_t size;

	enum RangeState { AVAILABLE, LOCKED, COMPLETE };
	std::vector<RangeState> range_states;

	std::size_t n_ranges;
	std::size_t n_ranges_complete;

	std::size_t range_index_lower;
	std::size_t range_index_upper;
	std::size_t range_index_upper_max;

	mutable std::mutex mutex;

	void on_range_complete(std::size_t range_index);

	friend std::unique_ptr<std::pair<std::size_t, std::size_t>> Range::get_next();
};
