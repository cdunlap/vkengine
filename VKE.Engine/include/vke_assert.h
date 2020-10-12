#pragma once
#include "vke_defs.h"

// Assertions
#ifdef ENABLE_ASSERTS
#include <iostream>

#if _MSC_VER
#define DBG_BREAK() __debugbreak()
#else
#define DBG_BREAK() __asm { int 3 }
#endif // _MSC_VER

void FORCEINLINE ASSERT_FAILURE(const char* expression, const char* message, const char* file, int line) {
	std::cerr << "Assertion failure: " << expression << ", message: " << message << ", file: " << file << ", line: " << line << std::endl;
}

#define ASSERT(expr) do { \
	if(expr) { \
	} else { \
		ASSERT_FAILURE(#expr, "", __FILE__, __LINE__); \
		DBG_BREAK(); \
	} \
} while(0)

#define ASSERT_MSG(expr, msg) do { \
	if(expr) { \
	} else { \
		ASSERT_FAILURE(#expr, #msg, __FILE__, __LINE__); \
		DBG_BREAK(); \
	} \
} while(0)

#ifdef _DEBUG
#define ASSERT_DEBUG(expr) do { \
	if(expr) { \
	} else { \
		ASSERT_FAILURE(#expr, "", __FILE__, __LINE__); \
		DBG_BREAK(); \
	} \
} while(0)

#else
#define ASSERT_DEBUG(expr)
#endif

#else
#define ASSERT(expr)
#define ASSERT_MSG(expr, msg)
#define ASSERT_DEBUG(expr)
#endif // ENABLE_ASSERTS