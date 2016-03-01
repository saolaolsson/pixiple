#include "shared.h"

#include "duplicate.h"
#include "resource.h"
#include "tests.h"
#include "window.h"

#include "shared/com.h"

#include <atomic>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <ShlObj.h>

#pragma comment(lib, "comctl32")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "propsys")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

std::vector<std::tr2::sys::path> scan(Window& window, const std::vector<ComPtr<IShellItem>>& shell_items);
std::vector<std::vector<Duplicate>> process(Window& window, const std::vector<std::tr2::sys::path>& paths);
std::vector<ComPtr<IShellItem>> compare(Window& window, const std::vector<std::vector<Duplicate>>& duplicate_categories);

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

static void app() {
	#define STRINGIZE_(i) L ## # i
	#define STRINGIZE(i) STRINGIZE_(i)
	std::wstring window_title = APP_NAME L" (release " STRINGIZE(APP_RELEASE) L")";
	#ifdef _DEBUG
	window_title += L" _DEBUG";
	#endif

	Window window{
		window_title, {800, 600},
		er = LoadIcon(er = GetModuleHandle(nullptr), MAKEINTRESOURCE(APP_ICON))};

	window.add_edge(0);
	window.add_edge(0);
	window.add_edge(1);
	window.add_edge(1);
	window.add_pane(0, 1, 2, 3, {0, 0, 0,0 }, false, false, Colour{0xfff8f8f8});

	auto items = browse(window.get_handle());

	for (;;) {
		std::vector<std::vector<Duplicate>> duplicate_categories{4};

		window.reset();

		if (!items.empty()) {
			auto paths = scan(window, items);
			if (window.quit_event_seen())
				return;

			duplicate_categories = process(window, paths);
			if (window.quit_event_seen())
				return;

			paths.clear();
		}

		window.set_drop_target(true);
		items = compare(window, duplicate_categories);
		window.set_drop_target(false);

		if (window.quit_event_seen())
			return;
	}
}

int WINAPI WinMain(__in HINSTANCE, __in_opt HINSTANCE, __in LPSTR, __in int) {
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
		OleUninitialize();
	} catch (ErrorCodeException& e) {
		die(e.line, e.file, e.hresult);
	} catch (...) {
		die();
	}

	TRACE();
	return 0;
}
