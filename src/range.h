#pragma once

#include <memory>

class RangePartitioning;

class Range {
public:
	Range(
		std::size_t first1, std::size_t last1, std::size_t first2, std::size_t last2)
		:
		range_partitioning{nullptr},
		range_partitioning_index{0},
		first1{first1}, last1{last1},
		first2{first2}, last2{last2},
		i1{first1}, i2{first2},
		overlap{false},
		done{false}
	{
		assert(first1 <= last1 && first2 <= last2); // well-ordered
		assert(last1 <= first2); // no overlap
	}

	Range(
		RangePartitioning& range_partitioning,
		std::size_t range_partitioning_index,
		std::size_t first,
		std::size_t last)
		:
		range_partitioning{&range_partitioning},
		range_partitioning_index{range_partitioning_index},
		first1{first}, last1{last},
		first2{first}, last2{last},
		i1{first}, i2{i1 + 1},
		overlap{true},
		done{false}
	{
		assert(first <= last); // well-ordered
	}

	std::unique_ptr<std::pair<std::size_t, std::size_t>> get_next();

private:
	RangePartitioning* range_partitioning;
	std::size_t range_partitioning_index;

	std::size_t first1;
	std::size_t last1;
	std::size_t first2;
	std::size_t last2;

	std::size_t i1;
	std::size_t i2;

	bool overlap;
	bool done;
};
