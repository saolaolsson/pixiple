#include "shared.h"

#include "range.h"

#include "range_partitioning.h"

#include <memory>

std::unique_ptr<std::pair<std::size_t, std::size_t>> Range::get_next() {
	if (i2 > last2)
		done = true;

	if (done) {
		if (range_partitioning) {
			range_partitioning->on_range_complete(range_partitioning_index);
			range_partitioning = nullptr;
		}
		return nullptr;
	}

	auto r = std::make_unique<std::pair<std::size_t, std::size_t>>(i1, i2);

	i2++;
	if (i2 > last2) {
		if (overlap) {
			i1++;
			if (i1 == last1)
				done = true;
			i2 = i1 + 1;
		} else {
			i1++;
			if (i1 > last1)
				done = true;
			i2 = first2;
		}
	}

	return r;
}
