#include "shared.h"

#include "hash.h"

#include <sstream>

std::wostream& operator<<(std::wostream& os, const Hash hash) {
	std::wostringstream ss;
	ss << std::hex << hash.hash[0] << hash.hash[1];
	return os << ss.str() << std::dec;
}
