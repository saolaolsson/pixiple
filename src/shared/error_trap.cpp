#include "error_trap.h"

#define LOGGING
#include "debug_log.h"

#include <mutex>
#include <sstream>

#include <comdef.h>

#ifdef DIRECTX
#include <dxerr.h>
#pragma comment(lib, "dxerr")
#endif

std::atomic<bool> ErrorTrap::stay_alive = false;
std::atomic<bool> ErrorTrap::good = true;

void ErrorTrap::die(HRESULT hr) const {
	good = false;

	if (SUCCEEDED(hr))
		hr = HRESULT_FROM_WIN32(GetLastError());

	std::wostringstream oss;

	wchar_t filename[MAX_PATH];
	DWORD r = GetModuleFileName(nullptr, filename, sizeof(filename) / sizeof(filename[0]));
	if (r != 0) {
		filename[sizeof(filename) / sizeof(filename[0]) - 1] = L'\0';
		oss << filename << L" exiting due to error ";
	} else {
		oss << "Error ";
	}

	oss << std::hex << L"0x" << hr << std::dec << L" in ";
	oss << file << L" on line " << line;

	if (FAILED(hr)) {
		_com_error error(hr);
		if (error.ErrorMessage())
			oss << L": " << error.ErrorMessage();
	}

	#ifdef DIRECTX
	std::wstring dxerr(DXGetErrorDescription(hr));
	if (dxerr != L"n/a")
		oss << L"\n\nDirectX error: " << dxerr;
	#endif

	#ifdef _DEBUG
	if (!stay_alive) {
		debug_log << oss.str() << std::endl;
		__debugbreak();
	}
	#else
	static std::mutex mutex;
	std::lock_guard<std::mutex> lg(mutex);
	MessageBox(nullptr, oss.str().c_str(), nullptr, MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
	_exit(EXIT_FAILURE); // exit with limited cleanup
	#endif
}
