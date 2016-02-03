#pragma once

#include <comip.h>

// CComPtr::operator&() asserts on non-null pointer.
// _com_ptr_t::operator&() releases a non-null pointer and provides an address.
// Neither is ideal but _com_ptr_t behaviour is less annoying when (re)creating interfaces.

template<typename T>
using ComPtr = _com_ptr_t<_com_IIID<T, &__uuidof(T)>>;
