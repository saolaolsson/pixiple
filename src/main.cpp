#include "shared.h"

#include "duplicate.h"
#include "resource.h"
#include "tests.h"
#include "window.h"

#include "shared/com.h"

#include <atomic>
#include <sstream>
#include <string>
#include <vector>

#include <ShlObj.h>

#pragma comment(lib, "comctl32")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "propsys")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

std::vector<std::wstring> scan(Window& window, ComPtr<IShellItem> root);
std::vector<std::vector<Duplicate>> process(Window& window, const std::vector<std::wstring>& paths);
void compare(Window& window, const std::vector<std::vector<Duplicate>>& duplicate_categories);

static ComPtr<IShellItem> browse() {
	PIDLIST_ABSOLUTE pidlist;

	#ifdef _DEBUG
	pidlist = et = ILCreateFromPath(L"c:\\users\\");
	#else
	BROWSEINFO bi{
		nullptr, nullptr, nullptr,
		L"Select a folder to scan for similar images (recursively, starting with the images and folders in the selected folder).",
		//BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON,
		//BIF_BROWSEFILEJUNCTIONS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON,
		BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON,
		nullptr, 0, 0};
	pidlist = SHBrowseForFolder(&bi);
	if (pidlist == nullptr)
		return nullptr;
	#endif

	ComPtr<IShellItem> root;
	et = SHCreateItemFromIDList(pidlist, IID_PPV_ARGS(&root));
	CoTaskMemFree(pidlist);

	return root;
}

static void app() {
	#define STRINGIZE_(i) L ## # i
	#define STRINGIZE(i) STRINGIZE_(i)
	std::wstring window_title = APP_NAME L" R" STRINGIZE(APP_RELEASE);
	#ifdef _DEBUG
	window_title += L" _DEBUG";
	#endif

	Window window(
		window_title, {800, 600},
		et = LoadIcon(et = GetModuleHandle(nullptr), MAKEINTRESOURCE(APP_ICON)));

	window.add_edge(0);
	window.add_edge(0);
	window.add_edge(1);
	window.add_edge(1);
	window.add_pane(0, 0, 1, 2, 3, {0, 0, 0,0 }, false, false, D2D1::ColorF(0xf8f8f8));

	for (;;) {
		window.reset();

		auto root_item = browse();

		std::vector<std::vector<Duplicate>> duplicate_categories(4);

		if (root_item != nullptr) {
			auto paths = scan(window, root_item);
			if (window.quit_event_seen())
				return;

			duplicate_categories = process(window, paths);
			if (window.quit_event_seen())
				return;

			paths.clear();
		}

		compare(window, duplicate_categories);
		if (window.quit_event_seen())
			return;
	}
}

int WINAPI WinMain(__in HINSTANCE, __in_opt HINSTANCE, __in LPSTR, __in int) {
	TRACE();

	#ifdef _DEBUG
	// _EM_OVERFLOW and _EM_UNDERFLOW (and _EM_DENORMAL?) require /fp:strict
	// _EM_INEXACT caused when a value cannot be represented exactly as a float
	unsigned int current_control;
	et = _controlfp_s(&current_control, 0, 0) == 0;
	et = _controlfp_s(&current_control, current_control & ~(_EM_INVALID | _EM_DENORMAL | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW), _MCW_EM) == 0;
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

	et = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	app();
	CoUninitialize();

	TRACE();
	return 0;
}
