#include "shared.h"

#include "time.h"

#include <cmath>
#include <iomanip>

std::wostream& operator<<(std::wostream& os, const std::chrono::system_clock::time_point tp) {
	std::wostringstream ss;
	ss.imbue(std::locale(""));

	auto t = std::chrono::system_clock::to_time_t(tp);
	tm tm;
	er = localtime_s(&tm, &t) == 0;
	ss << std::put_time(&tm, L"%c");

	return os << ss.str();
}

std::wostream& operator<<(std::wostream& os, const std::chrono::system_clock::duration d) {
	auto ds = std::chrono::duration_cast<std::chrono::seconds>(d).count();
	std::wostringstream ss;
	if (d > 2*365*24h)
		ss << static_cast<decltype(ds)>(std::round(ds / 3600.0f / 24 / 365)) << L" years";
	else if (d > 365*24h)
		ss << static_cast<decltype(ds)>(std::round(ds / 3600.0f / 24 / 30)) << L" months";
	else if (d > 2*24h)
		ss << static_cast<decltype(ds)>(std::round(ds / 3600.0f / 24)) << L" days";
	else if (d > 2h)
		ss << static_cast<decltype(ds)>(std::round(ds / 3600.0f)) << L" hours";
	else if (d > 2min)
		ss << static_cast<decltype(ds)>(std::round(ds / 60.0f)) << L" minutes";
	else
		ss << ds << L" second" << (ds != 1 ? L"s" : L"");
	return os << ss.str();
}
