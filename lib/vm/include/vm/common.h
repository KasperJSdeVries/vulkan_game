#ifndef VM_COMMON_H_INCLUDED
#define VM_COMMON_H_INCLUDED

#include <math.h>
#include <quadmath.h>

#if defined(_MSC_VER)
#ifdef VM_STATIC
#define VM_EXPORT
#elif defined(VM_EXPORTS)
#define VM_EXPORT __declspec(dllexport)
#else
#define VM_EXPORT __declspec(dllimport)
#endif
#define VM_INLINE __forceinline
#else
#define VM_EXPORT __attribute__((visibility("default")))
#define VM_INLINE static inline __attribute((always_inline))
#endif

#if defined(__GNUC__) || defined(__clang__)
#define VM_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define VM_LIKELY(expr) __builtin_expect(!!(expr), 1)
#else
#define VM_UNLIKELY(expr) (expr)
#define VM_LIKELY(expr) (expr)
#endif

#include "types.h"

#endif // VM_COMMON_H_INCLUDED
