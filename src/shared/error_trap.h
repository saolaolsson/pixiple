#pragma once

#include <atomic>

#include <comdef.h>
#include <winerror.h>

#define et ErrorTrap(__LINE__, __FILE__)

class ErrorTrap {
private:
	static std::atomic<bool> stay_alive;
	static std::atomic<bool> good;

	const long line;
	const char* const file;

	#ifndef _DEBUG
	void __declspec(noreturn) die(HRESULT hr) const;
	#else
	void die(HRESULT hr) const;
	#endif

public:
	static void set_stay_alive(bool stay_alive_) {
		ErrorTrap::stay_alive = stay_alive_;
	}

	static bool is_good() {
		return good;
	}

	static bool is_good_and_reset() {
		bool r = good;
		good = true;
		return r;
	}

	ErrorTrap(const long line, const char* const file) : line{line}, file{file} { }

	template<typename T>
	const T& operator=(const T& t) const {
		if (t) {
			return t;
		} else {
			die(S_OK);
			#ifdef _DEBUG
			return t;
			#endif
		}
	}

	const HRESULT& operator=(const HRESULT& hr) const {
		if (SUCCEEDED(hr)) {
			return hr;
		} else {
			die(hr);
			#ifdef _DEBUG
			return hr;
			#endif
		}
	}

	const HANDLE& operator=(const HANDLE& h) const {
		if (h != nullptr && h != INVALID_HANDLE_VALUE) {
			return h;
		} else {
			die(S_OK);
			#ifdef _DEBUG
			return h;
			#endif
		}
	}
};
