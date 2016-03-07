#include "shared.h"

#include "drop_target.h"

#include "window.h"

#include "shared/numeric_cast.h"

#include <vector>

#include <shellapi.h>

ComPtr<IDropTarget> create_drop_target(Window& window) {
	return new DropTarget(window);
}

HRESULT __stdcall DropTarget::QueryInterface(REFIID iid, void** object) {
	if (iid == IID_IDropTarget || iid == IID_IUnknown) {
		*object = this;
		AddRef();
		return S_OK;
	} else {
		*object = nullptr;
		return E_NOINTERFACE;
	}
}

ULONG __stdcall DropTarget::AddRef() {
	return ++n_refs;
}	

ULONG __stdcall DropTarget::Release() {
	n_refs--;
	if (n_refs == 0)
		delete this;
	return n_refs;
}

std::vector<ComPtr<IShellItem>> get_shell_items(IDataObject* object) {
	auto format = FORMATETC{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	STGMEDIUM stgm;
	if (FAILED(object->GetData(&format, &stgm)))
		return {};

	auto hdrop = reinterpret_cast<HDROP>(stgm.hGlobal);
	auto n_paths = DragQueryFile(hdrop, 0xffffffff, nullptr, 0);

	std::vector<ComPtr<IShellItem>> items;
	for (UINT i = 0; i < n_paths; i++) {
		auto buffer_size = er = DragQueryFile(hdrop, i, nullptr, 0);
		std::vector<wchar_t> buffer(buffer_size + 1);
		buffer[buffer_size] = L'\0';
		er = DragQueryFile(hdrop, i, buffer.data(), numeric_cast<UINT>(buffer.size()));

		ComPtr<IShellItem> si;
		er = SHCreateItemFromParsingName(
			buffer.data(), nullptr, IID_IShellItem, reinterpret_cast<void**>(&si));
		items.push_back(si);
	}
	ReleaseStgMedium(&stgm);

	return items;
}

HRESULT __stdcall DropTarget::DragEnter(IDataObject* object, DWORD, POINTL, DWORD*) {
	drop_enabled = !get_shell_items(object).empty();
	return S_OK;
}

HRESULT __stdcall DropTarget::DragOver(DWORD, POINTL, DWORD* effect) {
	if (drop_enabled)
		*effect = DROPEFFECT_COPY;
	else
		*effect = DROPEFFECT_NONE;
	return S_OK;
}

HRESULT __stdcall DropTarget::DragLeave() {
	return S_OK;
}

HRESULT __stdcall DropTarget::Drop(IDataObject* object, DWORD, POINTL, DWORD*) {
	Event e{Event::Type::items};
	e.items = get_shell_items(object);
	if (!e.items.empty())
		window.queue_event(e);
	return S_OK;
}
