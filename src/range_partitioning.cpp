#include "shared.h"

#include "range_partitioning.h"

#include "range.h"

#include <algorithm>

RangePartitioning::RangePartitioning(std::size_t size) {
	this->size = size;

	n_ranges = size / range_size;
	n_ranges += (size % range_size != 0); // include partial range at the end, if any

	for (std::size_t i = 0; i < n_ranges; i++)
		range_states.push_back(AVAILABLE);

	range_index_upper_max = 0;
	range_index_upper = 0;
	range_index_lower = 0;

	n_ranges_complete = 0;
}

std::unique_ptr<Range> RangePartitioning::get_next() {
	std::lock_guard<std::mutex> lg(mutex);

	if (range_index_lower < range_index_upper) {
		// increase range_index_lower

		auto first1 = range_index_lower*range_size;
		auto last1 = std::min(first1 + range_size - 1, size - 1);
		auto first2 = range_index_upper*range_size;
		auto last2 = std::min(first2 + range_size - 1, size - 1);
		range_index_lower++;

		return std::make_unique<Range>(first1, last1, first2, last2);
	} else if (range_index_upper < range_index_upper_max) {
		// increase range_index_upper

		range_index_upper++;
		range_index_lower = 0;

		auto first1 = range_index_lower*range_size;
		auto last1 = std::min(first1 + range_size - 1, size - 1);
		auto first2 = range_index_upper*range_size;
		auto last2 = std::min(first2 + range_size - 1, size - 1);
		range_index_lower++;

		return std::make_unique<Range>(first1, last1, first2, last2);
	} else {
		// increase range_index_upper_max

		for (std::size_t i = range_index_upper_max; i < n_ranges; i++) {
			if (range_states[i] == AVAILABLE) {
				range_states[i] = LOCKED;

				auto first = i*range_size;
				auto last = std::min(first + range_size - 1, size - 1);

				return std::make_unique<Range>(*this, i, first, last);
			}
		}

		return nullptr;
	}
}

std::size_t RangePartitioning::get_size() const {
	std::lock_guard<std::mutex> lg(mutex);
	return n_ranges;
}

std::size_t RangePartitioning::get_progress() const {
	std::lock_guard<std::mutex> lg(mutex);
	return n_ranges_complete;
}

void RangePartitioning::on_range_complete(std::size_t range_index) {
	assert(range_index < n_ranges);
	assert(range_states[range_index] == LOCKED);

	std::lock_guard<std::mutex> lg(mutex);
	range_states[range_index] = COMPLETE;

	for (std::size_t i = range_index_upper_max + 1; i < n_ranges; i++) {
		if (range_states[i] == COMPLETE)
			range_index_upper_max = i;
		else
			break;
	}

	n_ranges_complete++;
}
