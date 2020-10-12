#pragma once

#if _WIN32 || _WIN64
	#define PLATFORM_WINDOWS
#elif __linux__
	#define PLATFORM_LINUX
#elif __APPLE__
	#define PLATFORM_MAC
#else
	#error "Unable to determine platform"
#endif

#ifdef PLATFORM_WINDOWS
	#define FORCEINLINE			__forceinline
	#define FORCENOINLINE		_declspec(noinline)
	#ifdef VKE_BUILD_LIB
		#define VKE_API	_declspec(dllexport)
	#else
		#define VKE_API _declspec(dllimport)
	#endif
#elif PLATFORM_LINUX || PLATFORM_MAC
#define FORCEINLINE	inline
#define FORCENOINLINE
	#ifdef VKE_BUILD_LIB
		#define VKE_API
	#else
		#define VKE_API
	#endif
#endif // PLATFORM_WINDOWS