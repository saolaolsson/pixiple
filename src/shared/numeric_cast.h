#pragma once

#include "error_reflector.h"

#include <limits>

// Four cases to test, since the signed type in a signed/unsigned comparison is cast to unsigned.
// TODO: use numeric_cast from Boost instead

template <typename To, typename From>
typename std::enable_if<std::is_signed<From>::value && std::is_signed<To>::value, To>::type numeric_cast(const From& from) {
	er = from >= std::numeric_limits<To>::min() && from <= std::numeric_limits<To>::max();
	return static_cast<To>(from);
}

template <typename To, typename From>
typename std::enable_if<std::is_unsigned<From>::value && std::is_signed<To>::value, To>::type numeric_cast(const From& from) {
	er = from <= static_cast<typename std::make_unsigned<To>::type>(std::numeric_limits<To>::max());
	return static_cast<To>(from);
}

template <typename To, typename From>
typename std::enable_if<std::is_signed<From>::value && std::is_unsigned<To>::value, To>::type numeric_cast(const From& from) {
	er = from >= 0 && static_cast<typename std::make_unsigned<From>::type>(from) <= std::numeric_limits<To>::max();
	return static_cast<To>(from);
}

template <typename To, typename From>
typename std::enable_if<std::is_unsigned<From>::value && std::is_unsigned<To>::value, To>::type numeric_cast(const From& from) {
	er = from <= std::numeric_limits<To>::max();
	return static_cast<To>(from);
}
