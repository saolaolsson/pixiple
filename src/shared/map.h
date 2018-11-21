#pragma once

template<typename T>
T sign(const T value) {
	if (value > 0)
		return 1;
	else if (value < 0)
		return -1;
	else
		return 0;
}

// wrap (not modulo) value into [0, max]
template<typename T>
T wrap(const T value, const T max) {
	assert(max > 0);

	T wrapped_value = static_cast<T>(fmod(value, max));
	if(wrapped_value < 0)
		wrapped_value += max;
	wrapped_value = std::abs(wrapped_value);

	assert(wrapped_value >= 0 && wrapped_value <= max);

	return wrapped_value; 
}

/*
Map values [input_0, input_n] to [output_0, output_n] using
piecewise linear interpolatation. Clamp values outside the input
interval to the output interval endpoints.
*/
template<typename TI, typename TO>
TO map(
	const TI value,
	const std::initializer_list<TI>& inputs,
	const std::initializer_list<TO>& ouputs
) {
	assert(inputs.size() >= 2);
	assert(inputs.size() == ouputs.size());

	auto ii = inputs.begin();
	auto oi = ouputs.begin();

	// lower clamp
	if (value <= *ii)
		return *oi;

	// piecewise interpolation
	while (ii+1 != inputs.end()) {
		const auto& i0 = *ii;
		const auto& o0 = *oi;
		const auto& i1 = *(ii+1);
		const auto& o1 = *(oi+1);
		if (value < i1)
			return o0 + (o1 - o0) * (value - i0) / (i1 - i0);

		ii++;
		oi++;
	}

	// upper clamp
	return *oi;
}
