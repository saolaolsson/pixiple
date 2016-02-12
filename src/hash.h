#pragma once

#include "external/murmurhash3.h"

#include "shared/numeric_cast.h"

#include <cstdint>

class Hash {
public:
	Hash() : hash{0, 0} {
	}

	Hash(const std::uint8_t* const data, const std::size_t length) {
		assert(data != nullptr);
		assert(length > 0);

		#ifdef _M_X64
			MurmurHash3_x64_128(data, numeric_cast<int>(length), 0, hash);
		#else
			MurmurHash3_x86_128(data, numeric_cast<int>(length), 0, hash);
		#endif

		assert(hash[0] != 0 && hash[1] != 0);
	}

	const bool operator==(const Hash& rhs) const {
		return hash[0] == rhs.hash[0] && hash[1] == rhs.hash[1];
	}

	friend std::wostream& operator<<(std::wostream& os, const Hash rhs);

private:
	std::uint64_t hash[2];
};
