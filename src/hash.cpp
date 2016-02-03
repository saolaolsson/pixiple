#include "shared.h"

#include "hash.h"

std::wostream& operator<<(std::wostream& os, const Hash hash) {
	std::wostringstream ss;
	ss << std::hex <<
		static_cast<std::uint16_t>(hash.hash[0] >> 48) << L"..." <<
		static_cast<std::uint16_t>(hash.hash[1]);
	return os << ss.str() << std::dec;
}
