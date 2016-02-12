#pragma once

#include "assert.h"

#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <streambuf>
#include <thread>

#ifdef LOGGING
#define debug_log debug_log_
#else
#define debug_log if (false, false) nullstream
#endif

#define TRACE() debug_log << __FUNCTION__ << L"@" << __LINE__ << std::endl;

struct NullStream { };
template <typename T>
NullStream& operator<<(NullStream& s, T const&) {
	return s;
}
NullStream& operator<<(NullStream& s, std::ostream&(std::ostream&));
extern NullStream nullstream;

class DebugLog;
extern DebugLog debug_log_;

class LogInterface : public std::wostream {
public:
	LogInterface() : std::wostream{nullptr} {
	}
	virtual void print_line(const std::wstring& string) = 0;
};

class StreamBuffer : public std::wstreambuf {
public:
	StreamBuffer(LogInterface* const instance) : instance{instance} {
		assert(instance);
	}

	~StreamBuffer() {
		assert(thread_buffers.empty());
	}

private:
	std::mutex mutex;
	LogInterface* const instance;
	std::map<std::thread::id, std::wstring> thread_buffers;
	
	virtual int_type overflow(int_type c = traits_type::eof()) {
		std::lock_guard<std::mutex> lg{mutex};

		// find existing or create new buffer string for this thread
		auto r = thread_buffers.insert(std::make_pair(std::this_thread::get_id(), L""));
		auto buffer_i = r.first;
		auto& buffer = buffer_i->second;

		// add character to buffer
		buffer += c;

		// if last character, output and destroy buffer
		if (c == L'\n' || c == traits_type::eof()) {
			instance->print_line(buffer);
			thread_buffers.erase(buffer_i);
		}

		return traits_type::not_eof(c);
	}

	StreamBuffer& operator=(const StreamBuffer&);
	StreamBuffer(const StreamBuffer&);
};

class DebugLog : public LogInterface {
public:
	DebugLog();
	~DebugLog();

	virtual void print_line(const std::wstring& string);

private:
	std::unique_ptr<StreamBuffer> stream_buffer;
	std::wofstream log_file;
};
