#include "shared.h"

#include "image_pair.h"
#include "resource.h"
#include "tests.h"
#include "window.h"

#include "shared/com.h"

#include <atomic>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <shellapi.h>
#include <ShlObj.h>

#pragma comment(lib, "comctl32")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "propsys")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

std::vector<std::tr2::sys::path> scan(Window& window, const std::vector<ComPtr<IShellItem>>& shell_items);
std::vector<std::vector<ImagePair>> process(Window& window, const std::vector<std::tr2::sys::path>& paths);
std::vector<ComPtr<IShellItem>> compare(Window& window, const std::vector<std::vector<ImagePair>>& pair_categories);

std::vector<ComPtr<IShellItem>> browse(HWND parent) {
	PIDLIST_ABSOLUTE pidlist;

	BROWSEINFO bi{
		parent, nullptr, nullptr,
		L"Select a folder to scan for similar images (recursively, starting with the images and folders in the selected folder).",
		BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON,
		nullptr, 0, 0};

	#ifdef _DEBUG
	pidlist = er = ILCreateFromPath(L"c:\\users\\");
	#else
	pidlist = SHBrowseForFolder(&bi);
	if (pidlist == nullptr)
		return {};
	#endif

	ComPtr<IShellItem> root;
	er = SHCreateItemFromIDList(pidlist, IID_PPV_ARGS(&root));
	CoTaskMemFree(pidlist);

	return {root};
}

std::vector<std::wstring> get_command_line_args() {
	wchar_t** args;
	int n_args;
	args = er = CommandLineToArgvW(GetCommandLine(), &n_args);

	std::vector<std::wstring> arg_strings;
	for (int i = 1; i < n_args; i++)
		arg_strings.push_back(args[i]);
		
	er = LocalFree(args) == nullptr;
	return arg_strings;
}

static void app() {
	#define STRINGIZE_(i) L ## # i
	#define STRINGIZE(i) STRINGIZE_(i)
	std::wstring window_title = APP_NAME L" " STRINGIZE(APP_RELEASE);
	#ifdef _DEBUG
	window_title += L" _DEBUG";
	#endif

	Window window{
		window_title, {800, 600},
		er = LoadIcon(er = GetModuleHandle(nullptr), MAKEINTRESOURCE(APP_ICON))};

	std::vector<ComPtr<IShellItem>> items;
	for (const auto& a : get_command_line_args()) {
		ComPtr<IShellItem> si;
		auto hr = SHCreateItemFromParsingName(a.data(), nullptr, IID_IShellItem, reinterpret_cast<void**>(&si));
		if (SUCCEEDED(hr))
			items.push_back(si);
	}
	if (items.empty())
		items = browse(window.get_handle());

	for (;;) {
		std::vector<std::vector<ImagePair>> pair_categories{4};

		window.reset();

		if (!items.empty()) {
			auto paths = scan(window, items);
			if (window.quit_event_seen())
				return;

			pair_categories = process(window, paths);
			if (window.quit_event_seen())
				return;

			paths.clear();
		}

		window.set_drop_target(true);
		items = compare(window, pair_categories);
		window.set_drop_target(false);

		if (window.quit_event_seen())
			return;
	}
}

int main() {
	TRACE();

	try {
		#ifdef _DEBUG
		// _EM_OVERFLOW and _EM_UNDERFLOW (and _EM_DENORMAL?) require /fp:strict
		// _EM_INEXACT caused when a value cannot be represented exactly as a float
		unsigned int current_control;
		er = _controlfp_s(&current_control, 0, 0) == 0;
		er = _controlfp_s(&current_control, current_control & ~(_EM_INVALID | _EM_DENORMAL | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW), _MCW_EM) == 0;
		_clearfp();
		#endif

		#ifdef _DEBUG
		auto dbg_flags = 0;
		dbg_flags |= _CRTDBG_ALLOC_MEM_DF;
		dbg_flags |= _CRTDBG_LEAK_CHECK_DF;
		//dbg_flags |= _CRTDBG_DELAY_FREE_MEM_DF; // Keep freed memory blocks in the heap's linked list, assign them the _FREE_BLOCK type, and fill them with the byte value 0xDD.
		//dbg_flags |= _CRTDBG_CHECK_ALWAYS_DF; // Call _CrtCheckMemory at every allocation and deallocation request.
		//dbg_flags |= _CRTDBG_CHECK_EVERY_128_DF;
		_CrtSetDbgFlag(dbg_flags);
		//_CrtSetBreakAlloc(2379);
		#endif

		#ifdef _DEBUG
		tests();
		#endif

		er = OleInitialize(nullptr);
		app();
		Image::clear_cache();
		OleUninitialize();
	} catch (ErrorCodeException& e) {
		die(e.line, e.file, e.hresult);
	} catch (...) {
		die();
	}

	TRACE();
	return 0;
}
