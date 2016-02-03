#pragma once

#pragma warning(disable: 4458) // warning C4458: declaration of 'var' hides class member

#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#endif

#ifdef _DEBUG
#define LOGGING
#endif

#include "shared/assert.h"
#include "shared/debug_log.h"
#include "shared/debug_timer.h"
#include "shared/error_trap.h"
