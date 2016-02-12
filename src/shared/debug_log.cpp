#include "debug_log.h"

#include "error_trap.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <io.h>
#include <fcntl.h>

NullStream nullstream;
DebugLog debug_log_;

NullStream& operator<<(NullStream& s, std::ostream&(std::ostream&)) { return s; }

static std::wstring get_time_string() {
	std::wostringstream ss;
	auto now = std::chrono::system_clock::now();

	// time (excluding milliseconds)
	auto t = std::chrono::system_clock::to_time_t(now);
	tm tm;
	et = localtime_s(&tm, &t) == 0;
	ss << std::put_time(&tm, L"%X");

	// milliseconds
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	ss << L"." << std::setw(3) << std::setfill(L'0') << ms % 1000;

	return ss.str();
}

static void redirect_console() {
	const auto n_console_lines = 1000;
	const auto n_console_rows = 120;

	et = AllocConsole();

	COORD size = {n_console_rows, n_console_lines};
	et = SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), size);

	SMALL_RECT sr = {0, 0, n_console_rows-1, 80};
	et = SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), true, &sr);

	int console;
	FILE* fp;
	int rc;

	// redirect stdout to console
	auto stdh = GetStdHandle(STD_OUTPUT_HANDLE);
	assert(stdh != INVALID_HANDLE_VALUE && stdh != nullptr);
	console = _open_osfhandle((intptr_t)stdh, _O_TEXT);
	assert(console != -1);
	fp = _fdopen(console, "w");
	assert(fp != nullptr);
	*stdout = *fp;
	rc = setvbuf(stdout, nullptr, _IONBF, 0);
	assert(rc == 0);

	// redirect stdin to console
	stdh = GetStdHandle(STD_INPUT_HANDLE);
	assert(stdh != INVALID_HANDLE_VALUE && stdh != nullptr);
	console = _open_osfhandle((intptr_t)stdh, _O_TEXT);
	assert(console != -1);
	fp = _fdopen(console, "r");
	assert(fp != nullptr);
	*stdin = *fp;
	rc = setvbuf(stdin, nullptr, _IONBF, 0);
	assert(rc == 0);

	// redirect stderr to console
	stdh = GetStdHandle(STD_ERROR_HANDLE);
	assert(stdh != INVALID_HANDLE_VALUE && stdh != nullptr);
	console = _open_osfhandle((intptr_t)stdh, _O_TEXT);
	assert(console != -1);
	fp = _fdopen(console, "w");
	assert(fp != nullptr);
	*stderr = *fp;
	rc = setvbuf(stderr, nullptr, _IONBF, 0);
	assert(rc == 0);

	bool b = std::ios::sync_with_stdio();
	assert(b);
}

DebugLog::DebugLog() {
	TRACE();

	stream_buffer = std::make_unique<StreamBuffer>(this);
	rdbuf(stream_buffer.get());

	log_file.open(L"debug_log.txt", std::ios_base::out | std::ios_base::app);
	assert(log_file);

	log_file << std::endl;
}

DebugLog::~DebugLog() {
	TRACE();
}

void DebugLog::print_line(const std::wstring& string) {
	std::wostringstream ss;
	ss << get_time_string() << L" " << string;

	log_file << ss.str();
	log_file.flush();
	assert(log_file);

	if (IsDebuggerPresent()) {
		OutputDebugString(ss.str().c_str());
	} else {
		static bool console_redirected = false;
		if (!console_redirected) {
			redirect_console();
			console_redirected = true;
		}

		std::wcout << ss.str();
		std::wcout.flush();
	}
}
