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
			std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

std::wstring trim_right(std::wstring s) {
	s.erase(
		std::find_if(
			s.rbegin(),
			s.rend(),
			std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

std::wstring trim(std::wstring s) {
	return trim_left(trim_right(s));
}
