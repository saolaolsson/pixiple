#pragma once

#undef assert

#ifdef NDEBUG
	#define assert(condition) ((void)0)
#else
	#define assert(condition) (void)((!!(condition)) || (__debugbreak(), 0))
#endif
