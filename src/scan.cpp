#include "shared.h"

#include "image.h"
#include "window.h"

#include "shared/com.h"

#include <filesystem>
#include <string>
#include <vector>

#include <ShlObj.h>

static bool is_image(const std::wstring& filename) {
	const std::wstring extensions[] {
		L".jpg", L".jpe", L".jpeg",
		L".png", L".gif", L".bmp"
		L".tif", L".tiff",
		L".jxr", L".hdp", L".wdp"};
	for (const auto& e : extensions) {
		if (filename.length() >= e.length()) {
			auto filename_e = filename.substr(filename.length() - e.length(), e.length());
			if (_wcsicmp(filename_e.c_str(), e.c_str()) == 0)
				return true;
		}
	}
	return false;
}

std::vector<std::tr2::sys::path> scan(Window& window, const std::vector<ComPtr<IShellItem>>& shell_items) {
	window.add_edge(0);
	window.add_edge(0);
	window.add_edge(1);
	window.add_edge(1);

	const auto mx = 12.0f;
	const auto my = 8.0f;
	const auto margin = D2D1::RectF(mx, my, mx, my);

	window.add_pane(0, 1, 2, 3, margin, false, false, Colour{0xfff8f8f8});
	window.set_cursor(0, IDC_WAIT);
	window.set_progressbar_progress(0, -1.0f);

	std::vector<std::tr2::sys::path> paths;
	auto items = shell_items;

	while (!items.empty()) {
		auto item = items.back();
		items.pop_back();

		SFGAOF a;
		er = item->GetAttributes(
			SFGAO_HIDDEN | SFGAO_FILESYSANCESTOR | SFGAO_FOLDER | SFGAO_FILESYSTEM, &a);

		// skip if not file or folder or if hidden
		if (!(a & SFGAO_FILESYSTEM || a & SFGAO_FILESYSANCESTOR) || a & SFGAO_HIDDEN)
			continue;

		if (a & SFGAO_FOLDER) {
			// folder
			ComPtr<IEnumShellItems> item_enum;
			er = item->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&item_enum));
			ComPtr<IShellItem> i;
			while (item_enum->Next(1, &i, nullptr) == S_OK)
				items.push_back(i);
		} else {
			// file
			LPWSTR path;
			er = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
			if (is_image(path))
				paths.push_back(path);
			CoTaskMemFree(path);
		}

		while (window.has_event())
			if (window.get_event().type == Event::Type::quit)
				return {};
	}

	debug_log << L"images found: " << paths.size() << std::endl;

	return paths;
}
