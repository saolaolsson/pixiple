#pragma once

#include <atomic>
#include <stdexcept>

#include <comdef.h>
#include <winerror.h>

#define er ErrorReflector(__LINE__, __FILE__)

void die(const long line = 0, const char* const filename = "", HRESULT hresult = S_OK);

struct ErrorCodeException : public std::runtime_error {
	long line;
	const char* file;
	HRESULT hresult;

	ErrorCodeException(
		const long line,
		const char* file,
		const HRESULT hresult = S_OK)
		:
		line{line},
		file{file},
		hresult{hresult},
		std::runtime_error{nullptr}
	{
	}
};

class ErrorReflector {
public:
	static bool is_good() {
		return good;
	}

	static bool is_good_and_reset() {
		bool r = good;
		good = true;
		return r;
	}

	#if _DEBUG
	static void quiesce(const bool quiesced_) {
		quiesced = quiesced_;
	}
	#endif

	ErrorReflector(const long line, const char* const file) : line{line}, file{file} {
	}

	template<typename T>
	const T& operator=(const T& t) const {
		if (t)
			return t;
		else
			throw_result(line, file);
		#if _DEBUG
		return t;
		#endif
	}

	const HRESULT& operator=(const HRESULT& hr) const {
		if (SUCCEEDED(hr))
			return hr;
		else
			throw_result(line, file, hr);
		#if _DEBUG
		return hr;
		#endif
	}

	const HANDLE& operator=(const HANDLE& h) const {
		if (h != nullptr && h != INVALID_HANDLE_VALUE)
			return h;
		else
			throw_result(line, file);
		#if _DEBUG
		return h;
		#endif
	}

private:
	static std::atomic<bool> good;
	#if _DEBUG
	static std::atomic<bool> quiesced;
	#endif

	const long line;
	const char* const file;

	#pragma warning(push)
	#pragma warning(disable: 4702) // warning C4702: unreachable code
	static void	throw_result(const long line, const char* const file, const HRESULT hresult = S_OK) {
		good = false;
		#if _DEBUG
		if (quiesced)
			return;
		#endif
		throw ErrorCodeException{line, file, hresult};
		#if _DEBUG
		good = true;
		#endif
	}
	#pragma warning(pop)
};
