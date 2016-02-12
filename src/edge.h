#pragma once

#include <algorithm>

class Edge {
public:
	Edge(float relative_position = -1)
		: relative_position{relative_position}, calculated_position{-1}
	{
		assert(relative_position == -1 || relative_position >= 0 && relative_position <= 1);
	}

	void reset_position() {
		calculated_position = -1;
	}

	void set_position(float absolute_position) {
		absolute_position = std::max(0.0f, absolute_position);
		calculated_position = absolute_position;
	}

	bool has_position() const {
		return is_fixed() || calculated_position != -1;
	}

	bool is_fixed() const {
		return relative_position != -1;
	}

	float get_position(float max_extent) {
		assert(max_extent >= 0);

		if (relative_position != -1) {
			return relative_position * max_extent;
		} else {
			return calculated_position;
		}
	}

private:
	float relative_position; // [0, 1] or -1 if unset
	float calculated_position;
};
