#include "error_reflector.h"

#define LOGGING
#include "debug_log.h"

#include <mutex>
#include <sstream>

#include <comdef.h>

#ifdef DIRECTX
#include <dxerr.h>
#pragma comment(lib, "dxerr")
#endif

std::atomic<bool> ErrorReflector::good = true;
#if _DEBUG
std::atomic<bool> ErrorReflector::quiesced = false;
#endif

#include <iomanip>

void die(const long line, const char* const filename, HRESULT hresult) {
	if (SUCCEEDED(hresult))
		hresult = HRESULT_FROM_WIN32(GetLastError());

	std::wostringstream oss;

	wchar_t name[MAX_PATH];
	auto r = GetModuleFileName(nullptr, name, sizeof name / sizeof name[0]);
	if (r != 0)
		name[sizeof name  / sizeof name[0] - 1] = L'\0';
	else 
		name[0] = '\0';

	if (line != 0 && filename != nullptr) {
		if (name[0])
			oss << name << L" exiting due to error ";
		else
			oss << L"Error ";
		oss << std::hex << L"0x" << std::setw(8) << std::setfill(L'0') << hresult << std::dec;
		oss << L" in " << filename << L" on line " << line;
	} else {
		if (name[0])
			oss << name << L" exiting due to unknown error";
		else
			oss << L"Unknown error";
	}

	if (FAILED(hresult)) {
		_com_error error{hresult};
		if (error.ErrorMessage())
			oss << L": " << error.ErrorMessage();
	}

	#ifdef DIRECTX
	std::wstring dxerr(DXGetErrorDescription(hr));
	if (dxerr != L"n/a")
		oss << L"\n\nDirectX error: " << dxerr;
	#endif

	#ifdef _DEBUG
		debug_log << oss.str() << std::endl;
		__debugbreak();
	#else
		static std::mutex mutex;
		std::lock_guard<std::mutex> lg{mutex};
		MessageBox(nullptr, oss.str().c_str(), nullptr, MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
		_exit(EXIT_FAILURE);
	#endif
}
