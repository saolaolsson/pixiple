#pragma once

#include "shared/com.h"

#include <atomic>

#include <oleidl.h>

class Window;

ComPtr<IDropTarget> create_drop_target(Window& window);

class DropTarget : public IDropTarget {
public:
	DropTarget(Window& window) : window{window} {
	}

	HRESULT __stdcall QueryInterface(REFIID iid, void** object);
	ULONG __stdcall AddRef();
	ULONG __stdcall Release();

	HRESULT __stdcall DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
	HRESULT __stdcall DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
	HRESULT __stdcall DragLeave();
	HRESULT __stdcall Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);

private:
	std::atomic<LONG> n_refs = 0;
	Window& window;
	bool drop_enabled = false;
};
