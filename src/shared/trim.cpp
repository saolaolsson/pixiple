#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

std::wstring trim_left(std::wstring s) {
	s.erase(
		s.begin(),
		std::find_if(
			s.begin(),
			s.end(),
			[](const auto& c) {
				return !std::isspace(c);
			}
		)
	);
	return s;
}

std::wstring trim_right(std::wstring s) {
	s.erase(
		std::find_if(
			s.rbegin(),
			s.rend(),
			[](const auto& c) {
				return !std::isspace(c);
			}
		).base(), s.end()
	);
	return s;
}

std::wstring trim(std::wstring s) {
	return trim_left(trim_right(s));
}
